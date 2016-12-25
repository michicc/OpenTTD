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
#include "vehicle_base.h"
#include "core/pool_func.hpp"
#include "aircraft.h"
#include "roadveh.h"
#include "ship.h"
#include "train.h"

/** The pool with all our consists. */
ConsistPool _consist_pool("Consist");
INSTANTIATE_POOL_METHODS(Consist)

Consist::Consist(VehicleType type)
{
	this->type = type;
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
 * The tick handler for consists.
 * @return True if the consist is still valid.
 */
bool Consist::Tick()
{
	Vehicle *v = this->Front();

	/* Update counters. */
	v->current_order_time++;
	if (!(v->vehstatus & VS_STOPPED) || v->cur_speed > 0) v->running_ticks++;

	/* Call movement function. */
	for (int i = 0; i < (this->type == VEH_TRAIN || this->type == VEH_AIRCRAFT ? 2 : 1); i++) {
		switch (this->type) {
			case VEH_TRAIN:
				if (!TrainLocoHandler(Train::From(v), i != 0)) return false;
				break;

			case VEH_ROAD:
				if (!RoadVehController(RoadVehicle::From(v))) return false;
				break;

			case VEH_SHIP:
				ShipController(Ship::From(v));
				break;

			case VEH_AIRCRAFT:
				if (!AircraftEventHandler(Aircraft::From(v), i)) return false;
				break;

			default: NOT_REACHED();
		}
	}

	return true;
}
