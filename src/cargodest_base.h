/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cargodest_base.h Classes and types for entities having cargo destinations. */

#ifndef CARGODEST_BASE_H
#define CARGODEST_BASE_H

#include "cargo_type.h"
#include "tile_type.h"
#include "town_type.h"
#include <vector>

struct CargoSourceSink;

/** Information about a demand link for cargo. */
struct CargoLink {
	CargoSourceSink                *dest;      //< Destination of the link.
	TransportedCargoStat<uint32_t> amount;     //< Transported cargo statistics.
	uint16_t                       weight;     //< Weight of this link.
	byte                           weight_mod; //< Weight modifier.

	CargoLink(CargoSourceSink *d = nullptr, byte mod = 1) : dest(d), weight(1), weight_mod(mod) {}

	/** Compare two cargo links for inequality. */
	bool operator !=(const CargoLink &other) const
	{
		return this->dest != other.dest;
	}

	/** Compare if this link refers to the given destination. */
	bool operator ==(const CargoSourceSink *other) const
	{
		return this->dest == other;
	}
};

/** An entity producing or accepting cargo with a destination. */
struct CargoSourceSink {
	/** List of destinations for each cargo type. */
	std::array<std::vector<CargoLink>, NUM_CARGO> cargo_links;
	/** NOSAVE: Sum of the destination weights for each cargo type. */
	std::array<uint, NUM_CARGO> cargo_links_weight;

	virtual ~CargoSourceSink();

	/** Update cached link weight sums. */
	void UpdateLinkWeightSums();

	/** Is this cargo accepted? */
	virtual bool IsCargoAccepted(CargoID cid) const = 0;
	/** Is this cargo produced? */
	virtual bool IsCargoProduced(CargoID cid) const = 0;

	/** Get the type of this entity. */
	virtual SourceType GetType() const = 0;
	/** Get the source ID corresponding with this entity. */
	virtual SourceID GetID() const = 0;
	/** Get the base map coordinate of this entity. */
	virtual TileIndex GetXY() const = 0;
};

#endif /* CARGODEST_BASE_H */
