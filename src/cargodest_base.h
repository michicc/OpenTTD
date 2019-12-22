/* $Id$ */

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
#include "order_type.h"
#include "station_type.h"
#include "company_type.h"
#include <vector>

struct CargoSourceSink;

/** Information about a demand link for cargo. */
struct CargoLink {
	CargoSourceSink              *dest;	     //< Destination of the link.
	TransportedCargoStat<uint32> amount;     //< Transported cargo statistics.
	uint                         weight;     //< Weight of this link.
	byte                         weight_mod; //< Weight modifier.

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
	std::vector<CargoLink> cargo_links[NUM_CARGO];
	/** NOSAVE: Sum of the destination weights for each cargo type. */
	uint cargo_links_weight[NUM_CARGO];

	/** NOSAVE: Desired link count for each cargo. */
	uint16 num_links_expected[NUM_CARGO];
	/** NOSAVE: Incoming link count for each cargo. */
	uint num_incoming_links[NUM_CARGO];

	virtual ~CargoSourceSink();

	/** Get the type of this entity. */
	virtual SourceType GetType() const = 0;
	/** Get the source ID corresponding with this entity. */
	virtual SourceID GetID() const = 0;
	/** Get the base map coordinate of this entity. */
	virtual TileIndex GetXY() const = 0;

	/** Is this cargo accepted? */
	virtual bool AcceptsCargo(CargoID cid) const = 0;
	/** Is this cargo produced? */
	virtual bool SuppliesCargo(CargoID cid) const = 0;

	/** Is there a link to the given destination for a cargo? */
	bool HasLinkTo(CargoID cid, const CargoSourceSink *dest) const
	{
		return std::find(this->cargo_links[cid].begin(), this->cargo_links[cid].end(), dest) != this->cargo_links[cid].end();
	}

	const CargoLink *GetRandomLink(CargoID cid, bool allow_self, bool allow_random = false, SourceType dst_type = ST_UNDEFINED) const;

	/** Get the link weight for this as a destination for a specific cargo. */
	virtual uint GetDestinationWeight(CargoID cid, byte weight_mod) const = 0;

	/** Update cached link weight sums. */
	void UpdateLinkWeightSums();

	/** Create the special cargo links for a cargo if not already present. */
	virtual void CreateSpecialLinks(CargoID cid);

	void SaveCargoSourceSink();
	void LoadCargoSourceSink();
	void PtrsCargoSourceSink();

	static CargoSourceSink *Get(SourceType type, SourceID id);
};

/** Holds information about a route service between two stations. */
struct RouteLink {
private:
	friend const struct SaveLoad *GetRouteLinkDescription(); ///< Saving and loading of route links.
	friend void ChangeOwnershipOfCompanyItems(Owner old_owner, Owner new_owner);

	StationID       dest;            ///< Destination station id.
	OrderID         prev_order;      ///< Id of the order the vehicle had when arriving at the origin.
	OrderID         next_order;      ///< Id of the order the vehicle will leave the station with.
	Owner           owner;           ///< Owner of the vehicle of the link.

public:
	RouteLink(StationID dest = INVALID_STATION, OrderID prev_order = INVALID_ORDER, OrderID next_order = INVALID_ORDER, Owner owner = INVALID_OWNER)
		: dest(dest), prev_order(prev_order), next_order(next_order), owner(owner)
	{}

	/** Get the target station of this link. */
	inline StationID GetDestination() const { return this->dest; }

	/** Get the order id that lead to the origin station. */
	inline OrderID GetOriginOrderId() const { return this->prev_order; }

	/** Get the order id that lead to the destination station. */
	inline OrderID GetDestOrderId() const { return this->next_order; }

	/** Get the owner of this link. */
	inline Owner GetOwner() const { return this->owner; }

	/** Update the destination of the route link. */
	inline void SetDestination(StationID dest_id, OrderID dest_order_id)
	{
		this->dest = dest_id;
		this->next_order = dest_order_id;
	}
};

/** Vector of route links. */
typedef std::vector<RouteLink> RouteLinks;

#endif /* CARGODEST_BASE_H */
