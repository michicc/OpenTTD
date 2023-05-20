/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file consist.cpp Base for all vehicle consist handling. */

#include "stdafx.h"
#include "consist_base.h"
#include "core/backup_type.hpp"
#include "core/pool_func.hpp"
#include "ai/ai.hpp"
#include "autoreplace_cmd.h"
#include "command_func.h"
#include "newgrf_engine.h"
#include "news_func.h"
#include "order_backup.h"
#include "station_map.h"
#include "strings_func.h"
#include "timetable.h"
#include "vehicle_base.h"
#include "vehicle_cmd.h"
#include "vehicle_func.h"
#include "vehicle_gui.h"
#include "window_func.h"

#include "safeguards.h"

/** The pool with all our consists. */
ConsistPool _consist_pool("Consist");
INSTANTIATE_POOL_METHODS(Consist)

Consist::~Consist()
{
	if (CleaningPool()) return;

	OrderBackup::ClearConsist(this);
	StopGlobalFollowConsist(this);
	DeleteOrderWarnings(this);
	DeleteConsistNews(this->index, INVALID_STRING_ID);
}

/**
 * Sets a new front vehicle pointer. This also updates the consist pointer of the vehicle chain.
 * @param front The new front vehicle.
 */
void Consist::SetFront(Vehicle *front)
{
	assert(front->IsPrimaryVehicle());
	assert(front->type == this->type);
	assert(front->owner == this->owner);

	this->front = front;
	front->SetConsist(this);
}

/**
 * The tick handler for consists.
 * @return True if the consist is still valid.
 */
bool Consist::Tick()
{
	Vehicle *v = this->Front();

	/* Update counters. */
	this->current_order_time++;
	if (!(v->vehstatus & VS_STOPPED) || v->cur_speed > 0) v->running_ticks++;

	return true;
}


/**
 * Handle the pathfinding result, especially the lost status.
 * If the vehicle is now lost and wasn't previously fire an
 * event to the AIs and a news message to the user. If the
 * vehicle is not lost anymore remove the news message.
 * @param path_found Whether the vehicle has a path to its destination.
 */
void Consist::HandlePathfindingResult(bool path_found)
{
	if (path_found) {
		/* Route found, is the vehicle marked with "lost" flag? */
		if (!HasBit(this->consist_flags, CF_PATHFINDER_LOST)) return;

		/* Clear the flag as the PF's problem was solved. */
		ClrBit(this->consist_flags, CF_PATHFINDER_LOST);
		SetWindowWidgetDirty(WC_VEHICLE_VIEW, this->Front()->index, WID_VV_START_STOP);
		InvalidateWindowClassesData(GetWindowClassForVehicleType(this->type));
		/* Delete the news item. */
		DeleteConsistNews(this->index, STR_NEWS_VEHICLE_IS_LOST);
		return;
	}

	/* Were we already lost? */
	if (HasBit(this->consist_flags, CF_PATHFINDER_LOST)) return;

	/* It is first time the problem occurred, set the "lost" flag. */
	SetBit(this->consist_flags, CF_PATHFINDER_LOST);
	SetWindowWidgetDirty(WC_VEHICLE_VIEW, this->Front()->index, WID_VV_START_STOP);
	InvalidateWindowClassesData(GetWindowClassForVehicleType(this->type));
	/* Notify user about the event. */
	AI::NewEvent(this->owner, new ScriptEventVehicleLost(this->Front()->index));
	if (_settings_client.gui.lost_vehicle_warn && this->owner == _local_company) {
		SetDParam(0, this->index);
		AddConsistAdviceNewsItem(STR_NEWS_VEHICLE_IS_LOST, this->index);
	}
}

/**
 * List of consists that should check for autoreplace this tick.
 * Mapping of consist -> leave depot immediately after autoreplace.
 */
using AutoreplaceMap = std::map<Consist *, bool>;
static AutoreplaceMap _consists_to_autoreplace;

void InitializeConsist()
{
	_consists_to_autoreplace.clear();
}

/**
 * Adds a consist to the list of consists that visited a depot this tick.
 * @param *cs consist to add
 */
void ConsistEnteredDepotThisTick(Consist *cs)
{
	Vehicle *v = cs->Front();

	/* Vehicle should stop in the depot if it was in 'stopping' state */
	_consists_to_autoreplace[cs] = !(v->vehstatus & VS_STOPPED);

	/* We ALWAYS set the stopped state. Even when the vehicle does not plan on
	 * stopping in the depot, so we stop it to ensure that it will not reserve
	 * the path out of the depot before we might autoreplace it to a different
	 * engine. The new engine would not own the reserved path we store that we
	 * stopped the vehicle, so autoreplace can start it again */
	v->vehstatus |= VS_STOPPED;
}

/**
 * Consist entirely entered the depot, update its status, orders, vehicle windows, service it, etc.
 */
void Consist::EnterDepot()
{
	Vehicle *v = this->Front();

	SetWindowDirty(WC_VEHICLE_VIEW, v->index);
	SetWindowDirty(WC_VEHICLE_DEPOT, v->tile);

	v->vehstatus |= VS_HIDDEN;
	v->cur_speed = 0;

	VehicleServiceInDepot(v);

	/* After a vehicle trigger, the graphics and properties of the vehicle could change. */
	TriggerVehicle(v, VEHICLE_TRIGGER_DEPOT);
	v->MarkDirty();

	InvalidateWindowData(WC_VEHICLE_VIEW, v->index);

	if (v->current_order.IsType(OT_GOTO_DEPOT)) {
		SetWindowDirty(WC_VEHICLE_VIEW, v->index);

		const Order *real_order = v->GetOrder(this->cur_real_order_index);

		/* Test whether we are heading for this depot. If not, do nothing.
		 * Note: The target depot for nearest-/manual-depot-orders is only updated on junctions, but we want to accept every depot. */
		if ((v->current_order.GetDepotOrderType() & ODTFB_PART_OF_ORDERS) &&
			real_order != nullptr && !(real_order->GetDepotActionType() & ODATFB_NEAREST_DEPOT) &&
			(v->type == VEH_AIRCRAFT ? v->current_order.GetDestination() != GetStationIndex(v->tile) : v->dest_tile != v->tile)) {
			/* We are heading for another depot, keep driving. */
			return;
		}

		if (v->current_order.IsRefit()) {
			Backup<CompanyID> cur_company(_current_company, v->owner, FILE_LINE);
			CommandCost cost = std::get<0>(Command<CMD_REFIT_VEHICLE>::Do(DC_EXEC, v->index, v->current_order.GetRefitCargo(), 0xFF, false, false, 0));
			cur_company.Restore();

			if (cost.Failed()) {
				_consists_to_autoreplace[this] = false;
				if (v->owner == _local_company) {
					/* Notify the user that we stopped the vehicle */
					SetDParam(0, this->index);
					AddConsistAdviceNewsItem(STR_NEWS_ORDER_REFIT_FAILED, this->index);
				}
			} else if (cost.GetCost() != 0) {
				v->profit_this_year -= cost.GetCost() << 8;
				if (v->owner == _local_company) {
					ShowCostOrIncomeAnimation(v->x_pos, v->y_pos, v->z_pos, cost.GetCost());
				}
			}
		}

		if (v->current_order.GetDepotOrderType() & ODTFB_PART_OF_ORDERS) {
			/* Part of orders */
			v->DeleteUnreachedImplicitOrders();
			UpdateVehicleTimetable(v, true);
			this->IncrementImplicitOrderIndex();
		}
		if (v->current_order.GetDepotActionType() & ODATFB_HALT) {
			/* Vehicles are always stopped on entering depots. Do not restart this one. */
			_consists_to_autoreplace[this] = false;
			/* Invalidate last_loading_station. As the link from the station
			 * before the stop to the station after the stop can't be predicted
			 * we shouldn't construct it when the vehicle visits the next stop. */
			v->last_loading_station = INVALID_STATION;
			if (this->owner == _local_company) {
				SetDParam(0, this->index);
				AddConsistAdviceNewsItem(STR_NEWS_TRAIN_IS_WAITING + this->type, this->index);
			}
			AI::NewEvent(v->owner, new ScriptEventVehicleWaitingInDepot(v->index));
		}
		v->current_order.MakeDummy();
	}
}

void CallConsistTicks()
{
	_consists_to_autoreplace.clear();

	for (Consist *cs : Consist::Iterate()) {
		cs->Tick();
	}

	Backup<CompanyID> cur_company(_current_company, FILE_LINE);
	for (auto &it : _consists_to_autoreplace) {
		Vehicle *v = it.first->Front();
		/* Autoreplace needs the current company set as the vehicle owner */
		cur_company.Change(v->owner);

		/* Start vehicle if we stopped them in ConsistEnteredDepotThisTick()
		 * We need to stop them between ConsistEnteredDepotThisTick() and here or we risk that
		 * they are already leaving the depot again before being replaced. */
		if (it.second) v->vehstatus &= ~VS_STOPPED;

		/* Store the position of the effect as the vehicle pointer will become invalid later */
		int x = v->x_pos;
		int y = v->y_pos;
		int z = v->z_pos;

		const Company *c = Company::Get(_current_company);
		SubtractMoneyFromCompany(CommandCost(EXPENSES_NEW_VEHICLES, (Money)c->settings.engine_renew_money));
		CommandCost res = Command<CMD_AUTOREPLACE_VEHICLE>::Do(DC_EXEC, v->index);
		SubtractMoneyFromCompany(CommandCost(EXPENSES_NEW_VEHICLES, -(Money)c->settings.engine_renew_money));

		if (!IsLocalCompany()) continue;

		if (res.Succeeded()) {
			ShowCostOrIncomeAnimation(x, y, z, res.GetCost());
			continue;
		}

		StringID error_message = res.GetErrorMessage();
		if (error_message == STR_ERROR_AUTOREPLACE_NOTHING_TO_DO || error_message == INVALID_STRING_ID) continue;

		if (error_message == STR_ERROR_NOT_ENOUGH_CASH_REQUIRES_CURRENCY) error_message = STR_ERROR_AUTOREPLACE_MONEY_LIMIT;

		StringID message;
		if (error_message == STR_ERROR_TRAIN_TOO_LONG_AFTER_REPLACEMENT) {
			message = error_message;
		} else {
			message = STR_NEWS_VEHICLE_AUTORENEW_FAILED;
		}

		SetDParam(0, it.first->index);
		SetDParam(1, error_message);
		AddConsistAdviceNewsItem(message, it.first->index);
	}

	cur_company.Restore();
}
