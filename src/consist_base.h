/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file consist_base.h Base class for all vehicle consists. */

#ifndef CONSIST_BASE_H
#define CONSIST_BASE_H

#include "consist_type.h"
#include "core/pool_type.hpp"
#include "vehicle_type.h"
#include "company_type.h"

/** A consist pool for a little over 1 million consists. */
typedef Pool<Consist, ConsistID, 512, 0xFF000> ConsistPool;
extern ConsistPool _consist_pool;

/** Consist data structure holding information common to a vehicle chain. */
class Consist : public ConsistPool::PoolItem<&_consist_pool> {
protected:
	Vehicle *front;             ///< Pointer to the first vehicle of the associated vehicle chain.

	Consist(VehicleType type = VEH_INVALID, ::Owner owner = INVALID_OWNER) : type(type), owner(owner) {}

public:
	VehicleType type; ///< Type of the consist.
	Owner owner;      ///< Which company owns the consist?

	/** We want to 'destruct' the right class. */
	virtual ~Consist();

	/**
	 * Gets the front vehicle of the associated vehicle chain.
	 * @return The front vehicle.
	 */
	inline Vehicle *Front() const { return this->front; }

	void SetFront(Vehicle *front);

	virtual bool Tick();

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
	 * Returns an iterable ensemble of all valid consists of type T
	 * @param from index of the first consist to consider
	 * @return an iterable ensemble of all valid consists of type T
	 */
	static Pool::IterateWrapper<T> Iterate(size_t from = 0) { return Pool::IterateWrapper<T>(from); }
};

#endif /* CONSIST_BASE_H */
