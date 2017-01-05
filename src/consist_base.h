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
#include "core/bitmath_func.hpp"
#include "base_consist.h"
#include "vehicle_base.h"


/** A consist pool for a little over 1 million consists. */
typedef Pool<Consist, ConsistID, 512, 0xFF000> ConsistPool;
extern ConsistPool _consist_pool;

/** Consist data structure holding information common to a vehicle chain. */
class Consist : public ConsistPool::PoolItem<&_consist_pool>, public BaseConsist {
private:
	Vehicle *front;             ///< Pointer to the first vehicle of the associated vehicle chain.

	void PreDestructor();

	bool HandleCrashedConsist(int loop);

	friend void Load_VEHS();
	friend bool LoadOldVehicle(struct LoadgameState *ls, int num);
	friend const struct SaveLoad *GetConsistDescription(); ///< So we can use private/protected variables in the saveload code

public:
	VehicleTypeByte type;  ///< Type of the consist.
	OwnerByte       owner; ///< Which company owns the consist?

	Consist(VehicleType type = VEH_INVALID);
	/** We want to 'destruct' the right class. */
	virtual ~Consist();

	bool Tick();

	/**
	 * Gets the front vehicle of the associated vehicle chain.
	 * @return The front vehicle.
	 */
	inline Vehicle *Front() const { return this->front; }

	void SetFront(Vehicle *front);

	/**
	 * Check if the vehicle is a ground vehicle.
	 * @return True iff the vehicle is a train or a road vehicle.
	 */
	inline bool IsGroundVehicle() const
	{
		return this->type == VEH_TRAIN || this->type == VEH_ROAD;
	}


	inline uint16 GetServiceInterval() const { return this->service_interval; }

	inline void SetServiceInterval(uint16 interval) { this->service_interval = interval; }

	inline bool ServiceIntervalIsCustom() const { return HasBit(this->consist_flags, CF_SERVINT_IS_CUSTOM); }

	inline bool ServiceIntervalIsPercent() const { return HasBit(this->consist_flags, CF_SERVINT_IS_PERCENT); }

	inline void SetServiceIntervalIsCustom(bool on) { SB(this->consist_flags, CF_SERVINT_IS_CUSTOM, 1, on); }

	inline void SetServiceIntervalIsPercent(bool on) { SB(this->consist_flags, CF_SERVINT_IS_PERCENT, 1, on); }

private:
	/**
	 * Advance cur_real_order_index to the next real order.
	 * cur_implicit_order_index is not touched.
	 */
	void SkipToNextRealOrderIndex()
	{
		if (this->Front()->GetNumManualOrders() > 0) {
			/* Advance to next real order */
			do {
				this->cur_real_order_index++;
				if (this->cur_real_order_index >= this->Front()->GetNumOrders()) this->cur_real_order_index = 0;
			} while (this->Front()->GetOrder(this->cur_real_order_index)->IsType(OT_IMPLICIT));
		} else {
			this->cur_real_order_index = 0;
		}
	}

public:
	/**
	 * Increments cur_implicit_order_index, keeps care of the wrap-around and invalidates the GUI.
	 * cur_real_order_index is incremented as well, if needed.
	 * Note: current_order is not invalidated.
	 */
	void IncrementImplicitOrderIndex()
	{
		if (this->cur_implicit_order_index == this->cur_real_order_index) {
			/* Increment real order index as well */
			this->SkipToNextRealOrderIndex();
		}

		assert(this->cur_real_order_index == 0 || this->cur_real_order_index < this->Front()->GetNumOrders());

		/* Advance to next implicit order */
		do {
			this->cur_implicit_order_index++;
			if (this->cur_implicit_order_index >= this->Front()->GetNumOrders()) this->cur_implicit_order_index = 0;
		} while (this->cur_implicit_order_index != this->cur_real_order_index && !this->Front()->GetOrder(this->cur_implicit_order_index)->IsType(OT_IMPLICIT));

		InvalidateConsistOrder(this, 0);
	}

	/**
	 * Advanced cur_real_order_index to the next real order, keeps care of the wrap-around and invalidates the GUI.
	 * cur_implicit_order_index is incremented as well, if it was equal to cur_real_order_index, i.e. cur_real_order_index is skipped
	 * but not any implicit orders.
	 * Note: current_order is not invalidated.
	 */
	void IncrementRealOrderIndex()
	{
		if (this->cur_implicit_order_index == this->cur_real_order_index) {
			/* Increment both real and implicit order */
			this->IncrementImplicitOrderIndex();
		} else {
			/* Increment real order only */
			this->SkipToNextRealOrderIndex();
			InvalidateConsistOrder(this, 0);
		}
	}

	/**
	 * Skip implicit orders until cur_real_order_index is a non-implicit order.
	 */
	void UpdateRealOrderIndex()
	{
		/* Make sure the index is valid */
		if (this->cur_real_order_index >= this->Front()->GetNumOrders()) this->cur_real_order_index = 0;

		if (this->Front()->GetNumManualOrders() > 0) {
			/* Advance to next real order */
			while (this->Front()->GetOrder(this->cur_real_order_index)->IsType(OT_IMPLICIT)) {
				this->cur_real_order_index++;
				if (this->cur_real_order_index >= this->Front()->GetNumOrders()) this->cur_real_order_index = 0;
			}
		} else {
			this->cur_real_order_index = 0;
		}
	}
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
