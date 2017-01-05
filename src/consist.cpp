/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file consist.cpp Base for all consist handling. */

#include "stdafx.h"
#include "consist_base.h"
#include "consist_func.h"
#include "vehicle_base.h"
#include "core/pool_func.hpp"
#include "core/backup_type.hpp"
#include "aircraft.h"
#include "roadveh.h"
#include "ship.h"
#include "train.h"
#include "vehicle_func.h"
#include "order_backup.h"
#include "company_base.h"
#include "company_func.h"
#include "command_func.h"
#include "strings_func.h"
#include "news_func.h"
#include "viewport_func.h"
#include "timetable.h"
#include "ai/ai.hpp"

#include "table/strings.h"

#include "safeguards.h"

/** The pool with all our consists. */
ConsistPool _consist_pool("Consist");
INSTANTIATE_POOL_METHODS(Consist)

/**
 * List of consist that should check for autoreplace this tick.
 * Mapping of consist -> leave depot immediately after autoreplace.
 */
typedef SmallMap<Consist *, bool, 4> AutoreplaceMap;
static AutoreplaceMap _consists_to_autoreplace;

Consist::Consist(VehicleType type)
{
	this->type = type;
}

/* virtual*/ Consist::~Consist()
{
	this->PreDestructor();

	if (CleaningPool()) return;

	DeleteConsistNews(this->index, INVALID_STRING_ID);
}

void Consist::PreDestructor()
{
	if (CleaningPool()) return;

	OrderBackup::ClearConsist(this);
	DeleteOrderWarnings(this);

	extern void StopGlobalFollowVehicle(const Consist *cs);
	StopGlobalFollowVehicle(this);

	DeleteWindowById(WC_VEHICLE_ORDERS, this->index);
	DeleteWindowById(WC_VEHICLE_REFIT, this->index);
	DeleteWindowById(WC_VEHICLE_DETAILS, this->index);
	DeleteWindowById(WC_VEHICLE_TIMETABLE, this->index);
}

/**
 * Sets a new front vehicle pointer. This also updates the consist pointer of the vehicle chain.
 * @param front The new front vehicle.
 */
void Consist::SetFront(Vehicle *front)
{
	assert(front->IsPrimaryVehicle());

	this->front = front;
	front->SetConsist(this);
}

/**
 * Handle a crashed consist.
 * @param loop Movement loop counter.
 * @return True if the consist is still valid.
 */
bool Consist::HandleCrashedConsist(int loop)
{
	switch (this->type) {
		case VEH_TRAIN:
			return loop > 0 ? true : HandleCrashedTrain(Train::From(this->Front()));

		case VEH_ROAD:
			return RoadVehIsCrashed(RoadVehicle::From(this->Front()));

		case VEH_AIRCRAFT:
			return HandleCrashedAircraft(Aircraft::From(this->Front()));

		default: return true;
	}
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

	/* Call movement function. */
	for (int i = 0; i < (this->type == VEH_TRAIN || this->type == VEH_AIRCRAFT ? 2 : 1); i++) {
		/* Check for crashed vehicles. */
		if (v->vehstatus & VS_CRASHED) return this->HandleCrashedConsist(i);
		if (this->type == VEH_ROAD && RoadVehCheckTrainCrash(RoadVehicle::From(v))) return this->HandleCrashedConsist(i);

		/* Handle breakdowns. */
		if (v->HandleBreakdown()) continue;
		if (this->type != VEH_TRAIN && (v->vehstatus & VS_STOPPED) && v->cur_speed == 0) continue;

		switch (this->type) {
			case VEH_TRAIN:
				if (!TrainLocoHandler(this, i != 0)) return false;
				break;

			case VEH_ROAD:
				if (!RoadVehController(this)) return false;
				break;

			case VEH_SHIP:
				ShipController(this);
				break;

			case VEH_AIRCRAFT:
				if (!AircraftEventHandler(this, i)) return false;
				break;

			default: NOT_REACHED();
		}
	}

	return true;
}


void InitializeConsist()
{
	_consists_to_autoreplace.Clear();
}

/**
 * Adds a vehicle to the list of vehicles that visited a depot this tick
 * @param *v vehicle to add
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
 * @param cs Consist that entered a depot.
 */
void ConsistEnterDepot(Consist *cs)
{
	Vehicle *v = cs->Front();

	switch (v->type) {
		case VEH_TRAIN: {
			Train *t = Train::From(v);
			SetWindowClassesDirty(WC_TRAINS_LIST);
			/* Clear path reservation */
			SetDepotReservation(t->tile, false);
			if (_settings_client.gui.show_track_reservation) MarkTileDirtyByTile(t->tile);

			UpdateSignalsOnSegment(t->tile, INVALID_DIAGDIR, t->owner);
			t->wait_counter = 0;
			t->force_proceed = TFP_NONE;
			ClrBit(t->flags, VRF_TOGGLE_REVERSE);
			t->ConsistChanged(CCF_ARRANGE);
			break;
		}

		case VEH_ROAD:
			SetWindowClassesDirty(WC_ROADVEH_LIST);
			break;

		case VEH_SHIP: {
			SetWindowClassesDirty(WC_SHIPS_LIST);
			Ship *ship = Ship::From(v);
			ship->state = TRACK_BIT_DEPOT;
			ship->UpdateCache();
			ship->UpdateViewport(true, true);
			SetWindowDirty(WC_VEHICLE_DEPOT, v->tile);
			break;
		}

		case VEH_AIRCRAFT:
			SetWindowClassesDirty(WC_AIRCRAFT_LIST);
			HandleAircraftEnterHangar(Aircraft::From(v));
			break;
		default: NOT_REACHED();
	}
	SetWindowDirty(WC_VEHICLE_VIEW, v->index);

	if (v->type != VEH_TRAIN) {
		/* Trains update the vehicle list when the first unit enters the depot and calls VehicleEnterDepot() when the last unit enters.
		 * We only increase the number of vehicles when the first one enters, so we will not need to search for more vehicles in the depot */
		InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
	}
	SetWindowDirty(WC_VEHICLE_DEPOT, v->tile);

	v->vehstatus |= VS_HIDDEN;
	v->cur_speed = 0;

	VehicleServiceInDepot(v);

	/* After a vehicle trigger, the graphics and properties of the vehicle could change. */
	TriggerVehicle(v, VEHICLE_TRIGGER_DEPOT);
	v->MarkDirty();

	if (v->current_order.IsType(OT_GOTO_DEPOT)) {
		SetWindowDirty(WC_VEHICLE_VIEW, v->index);

		const Order *real_order = v->GetOrder(cs->cur_real_order_index);

		/* Test whether we are heading for this depot. If not, do nothing.
		 * Note: The target depot for nearest-/manual-depot-orders is only updated on junctions, but we want to accept every depot. */
		if ((v->current_order.GetDepotOrderType() & ODTFB_PART_OF_ORDERS) &&
				real_order != NULL && !(real_order->GetDepotActionType() & ODATFB_NEAREST_DEPOT) &&
				(v->type == VEH_AIRCRAFT ? v->current_order.GetDestination() != GetStationIndex(v->tile) : v->dest_tile != v->tile)) {
			/* We are heading for another depot, keep driving. */
			return;
		}

		if (v->current_order.IsRefit()) {
			Backup<CompanyByte> cur_company(_current_company, v->owner, FILE_LINE);
			CommandCost cost = DoCommand(v->tile, v->index, v->current_order.GetRefitCargo() | 0xFF << 8, DC_EXEC, GetCmdRefitVeh(v));
			cur_company.Restore();

			if (cost.Failed()) {
				_consists_to_autoreplace[cs] = false;
				if (v->owner == _local_company) {
					/* Notify the user that we stopped the vehicle */
					SetDParam(0, cs->index); // Special string param handling in news GUI code.
					AddConsistAdviceNewsItem(STR_NEWS_ORDER_REFIT_FAILED, cs->index);
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
			cs->IncrementImplicitOrderIndex();
		}
		if (v->current_order.GetDepotActionType() & ODATFB_HALT) {
			/* Vehicles are always stopped on entering depots. Do not restart this one. */
			_consists_to_autoreplace[cs] = false;
			/* Invalidate last_loading_station. As the link from the station
			 * before the stop to the station after the stop can't be predicted
			 * we shouldn't construct it when the vehicle visits the next stop. */
			v->last_loading_station = INVALID_STATION;
			if (v->owner == _local_company) {
				SetDParam(0, cs->index);  // Special string param handling in news GUI code.
				AddConsistAdviceNewsItem(STR_NEWS_TRAIN_IS_WAITING + cs->type, cs->index);
			}
			AI::NewEvent(v->owner, new ScriptEventVehicleWaitingInDepot(v->index));
		}
		v->current_order.MakeDummy();
	}
}

/**
 * Call all consist tick (and other date-related) handlers.
 */
void CallConsistTicks()
{
	_consists_to_autoreplace.Clear();

	Consist *cs;
	FOR_ALL_CONSISTS(cs) {
		cs->Tick();
	}

	Backup<CompanyByte> cur_company(_current_company, FILE_LINE);
	for (AutoreplaceMap::iterator it = _consists_to_autoreplace.Begin(); it != _consists_to_autoreplace.End(); it++) {
		Vehicle *v = it->first->Front();
		/* Autoreplace needs the current company set as the vehicle owner */
		cur_company.Change(v->owner);

		/* Start vehicle if we stopped them in VehicleEnteredDepotThisTick()
		* We need to stop them between VehicleEnteredDepotThisTick() and here or we risk that
		* they are already leaving the depot again before being replaced. */
		if (it->second) v->vehstatus &= ~VS_STOPPED;

		/* Store the position of the effect as the vehicle pointer will become invalid later */
		int x = v->x_pos;
		int y = v->y_pos;
		int z = v->z_pos;

		const Company *c = Company::Get(_current_company);
		SubtractMoneyFromCompany(CommandCost(EXPENSES_NEW_VEHICLES, (Money)c->settings.engine_renew_money));
		CommandCost res = DoCommand(0, v->index, 0, DC_EXEC, CMD_AUTOREPLACE_VEHICLE);
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

		SetDParam(0, it->first->index); // Special string param handling in news GUI code.
		SetDParam(1, error_message);
		AddConsistAdviceNewsItem(message, it->first->index);
	}

	cur_company.Restore();
}
