/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file consist.cpp Base for all vehicle consist handling. */

#include "stdafx.h"
#include "consist_base.h"
#include "core/pool_func.hpp"
#include "vehicle_base.h"

#include "safeguards.h"

/** The pool with all our consists. */
ConsistPool _consist_pool("Consist");
INSTANTIATE_POOL_METHODS(Consist)

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
	v->current_order_time++;
	if (!(v->vehstatus & VS_STOPPED) || v->cur_speed > 0) v->running_ticks++;

	return true;
}
