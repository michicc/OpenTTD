/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file cargodest_base.h Classes and types for entities having cargo destinations. */

#ifndef CARGODEST_BASE_H
#define CARGODEST_BASE_H

#include "cargo_type.h"
#include "source_type.h"
#include "tile_type.h"

struct CargoSourceSink;

/** Information about a demand link for cargo. */
struct CargoDemandLink {
	CargoSourceSink *dest; //< Destination of the link.
	TransportedCargoStat<uint32_t> amount; //< Transported cargo statistics.
	uint16_t weight; //< Weight of this link.
	uint8_t weight_mod; //< Weight modifier.

	CargoDemandLink(CargoSourceSink *dest = nullptr, uint8_t weight_mod = 1) : dest(dest), weight(1), weight_mod(weight_mod) {}

	/** Compare two cargo links for inequality. */
	bool operator !=(const CargoDemandLink &other)
	{
		return this->dest != other.dest;
	}
};

/** An entity producing or accepting cargo with a destination. */
struct CargoSourceSink {
	/** List of destinations for each cargo type. */
	std::array<std::vector<CargoDemandLink>, NUM_CARGO> cargo_links;
	/** NOSAVE: Sum of the destination weights for each cargo type. */
	std::array<uint, NUM_CARGO> cargo_links_weight;

	/** Update cached link weight sums. */
	void UpdateLinkWeightSums();

	/** Get the source of this entity. */
	virtual Source ToSource() const = 0;
	/** Get the base map coordinate of this entity. */
	virtual TileIndex GetXY() const = 0;
};

template <typename T>
concept CargoSourceSinkObject = std::is_base_of_v<CargoSourceSink, T>;

#endif /* CARGODEST_BASE_H */
