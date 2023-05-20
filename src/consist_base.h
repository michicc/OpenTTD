/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file consist_base.h Base class for all vehicle consists. */

#ifndef CONSIST_BASE_H
#define CONSIST_BASE_H

#include "base_consist.h"
#include "consist_type.h"
#include "core/bitmath_func.hpp"
#include "core/pool_type.hpp"
#include "vehicle_base.h"
#include "company_type.h"

/** A consist pool for a little over 1 million consists. */
typedef Pool<Consist, ConsistID, 512, 0xFF000> ConsistPool;
extern ConsistPool _consist_pool;

/** Consist data structure holding information common to a vehicle chain. */
class Consist : public ConsistPool::PoolItem<&_consist_pool>, public BaseConsist {
protected:
	Vehicle *front;             ///< Pointer to the first vehicle of the associated vehicle chain.

	Consist(VehicleType type = VEH_INVALID, ::Owner owner = INVALID_OWNER) : type(type), owner(owner) {}
	friend struct CNSTChunkHandler;

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
	VehicleType type; ///< Type of the consist.
	Owner owner;      ///< Which company owns the consist?

	TimerGameTick::TickCounter last_loading_tick;         ///< Last time (based on TimerGameTick counter) the vehicle has stopped at a station and could possibly leave with any cargo loaded.

	/** We want to 'destruct' the right class. */
	virtual ~Consist();

	/**
	 * Gets the front vehicle of the associated vehicle chain.
	 * @return The front vehicle.
	 */
	inline Vehicle *Front() const { return this->front; }

	void SetFront(Vehicle *front);


	inline uint16_t GetServiceInterval() const { return this->service_interval; }

	inline void SetServiceInterval(uint16_t interval) { this->service_interval = interval; }

	inline bool ServiceIntervalIsCustom() const { return HasBit(this->consist_flags, CF_SERVINT_IS_CUSTOM); }

	inline bool ServiceIntervalIsPercent() const { return HasBit(this->consist_flags, CF_SERVINT_IS_PERCENT); }

	inline void SetServiceIntervalIsCustom(bool on) { SB(this->consist_flags, CF_SERVINT_IS_CUSTOM, 1, on); }

	inline void SetServiceIntervalIsPercent(bool on) { SB(this->consist_flags, CF_SERVINT_IS_PERCENT, 1, on); }

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

		InvalidateVehicleOrder(this->Front(), 0);
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
			InvalidateVehicleOrder(this->Front(), 0);
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


	virtual bool Tick();

	virtual void EnterDepot();

	/**
	 * Returns an iterable ensemble of all valid consists.
	 * @param from index of the first consist to consider.
	 * @return an iterable ensemble of all valid consists.
	 */
	static Pool::IterateWrapper<Consist> Iterate(size_t from = 0) { return Pool::IterateWrapper<Consist>(from); }

	friend struct VEHSChunkHandler;
	friend bool LoadOldVehicle(struct LoadgameState *ls, int num);
};

/**
 * Templated base class for consists of a specific transport type.
 */
template <class T, class Tveh, VehicleType TType>
struct SpecializedConsist : public Consist {
	static const VehicleType EXPECTED_TYPE = TType; ///< Specialized type
	static_assert(EXPECTED_TYPE == Tveh::EXPECTED_TYPE);

protected:
	typedef SpecializedConsist<T, Tveh, TType> SpecializedConsistBase; ///< Our type

	inline SpecializedConsist(::Owner owner = INVALID_OWNER) : Consist(TType, owner) {}

public:
	/**
	 * Gets the front vehicle of the associated vehicle chain.
	 * @return The front vehicle.
	 */
	inline Tveh *Front() const
	{
		return static_cast<Tveh *>(this->front);
	}

	/**
	 * Tests whether given index is a valid index for consist of this type
	 * @param index tested index
	 * @return is this index valid index of T?
	 */
	static inline bool IsValidID(size_t index)
	{
		return Consist::IsValidID(index) && Consist::Get(index)->type == TType;
	}

	/**
	 * Gets consist with given index
	 * @return pointer to consist with given index casted to T *
	 */
	static inline T *Get(size_t index)
	{
		return static_cast<T *>(Consist::Get(index));
	}

	/**
	 * Returns consist if the index is a valid index for this consist type
	 * @return pointer to consist with given index if it's a consist of this type
	 */
	static inline T *GetIfValid(size_t index)
	{
		return IsValidID(index) ? Get(index) : nullptr;
	}

	/**
	 * Converts a Consist to SpecializedConsist with type checking.
	 * @param cs Consist pointer
	 * @return pointer to SpecializedConsist
	 */
	static inline T *From(Consist *cs)
	{
		assert(cs == nullptr || cs->type == TType);
		return (T *)cs;
	}

	/**
	 * Converts a const Consist to const SpecializedConsist with type checking.
	 * @param cs Consist pointer
	 * @return pointer to SpecializedConsist
	 */
	static inline const T *From(const Consist *cs)
	{
		assert(cs == nullptr || cs->type == TType);
		return (const T *)cs;
	}

	/**
	 * Returns an iterable ensemble of all valid consists of type T
	 * @param from index of the first consist to consider
	 * @return an iterable ensemble of all valid consists of type T
	 */
	static Pool::IterateWrapper<T> Iterate(size_t from = 0) { return Pool::IterateWrapper<T>(from); }
};

#endif /* CONSIST_BASE_H */
