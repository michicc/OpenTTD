/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file consist_base.h Base class for all consists. */

#ifndef CONSIST_BASE_H
#define CONSIST_BASE_H

#include "consist_type.h"
#include "vehicle_type.h"
#include "core/pool_type.hpp"

/** A consist pool for a little over 1 million consists. */
typedef Pool<Consist, ConsistID, 512, 0xFF000> ConsistPool;
extern ConsistPool _consist_pool;

/** Consist data structure holding information common to a vehicle chain. */
class Consist : public ConsistPool::PoolItem<&_consist_pool> {
private:
	Vehicle *front;             ///< Pointer to the first vehicle of the associated vehicle chain.

	friend void Load_VEHS();
	friend bool LoadOldVehicle(struct LoadgameState *ls, int num);

public:
	VehicleTypeByte type; ///< Type of the consist.

	Consist(VehicleType type = VEH_INVALID);
	/** We want to 'destruct' the right class. */
	virtual ~Consist() {}

	/**
	 * Gets the front vehicle of the associated vehicle chain.
	 * @return The front vehicle.
	 */
	inline Vehicle *Front() const { return this->front; }

	void SetFront(Vehicle *front);
};

/**
 * Iterate over all consists from a given point.
 * @param var   The variable used to iterate over.
 * @param start The consist to start the iteration at.
 */
#define FOR_ALL_CONSISTS_FROM(var, start) FOR_ALL_ITEMS_FROM(Consist, consist_index, var, start)

/**
 * Iterate over all consists.
 * @param var The variable used to iterate over.
 */
#define FOR_ALL_CONSISTS(var) FOR_ALL_CONSISTS_FROM(var, 0)

#endif /* CONSIST_BASE_H */
