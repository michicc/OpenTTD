/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file depot_map.h Map related accessors for depots. */

#ifndef DEPOT_MAP_H
#define DEPOT_MAP_H

#include "station_map.h"

/**
 * Check if a tile is a depot and it is a depot of the given type.
 */
static inline bool IsDepotTypeTile(TileIndex tile, TransportType type)
{
	switch (type) {
		default: NOT_REACHED();
		case TRANSPORT_RAIL:
			return IsRailDepotTile(tile);

		case TRANSPORT_ROAD:
			return IsRoadDepotTile(tile);

		case TRANSPORT_WATER:
			return IsShipDepotTile(tile);

		case TRANSPORT_AIR:
			return IsHangarTile(tile);
	}
}

/**
 * Is the given tile a tile with a depot on it?
 * @param tile the tile to check
 * @return true if and only if there is a depot on the tile.
 */
static inline bool IsDepotTile(TileIndex tile)
{
	return IsRailDepotTile(tile) || IsRoadDepotTile(tile) || IsShipDepotTile(tile) || IsHangarTile(tile);
}

/**
 * Get the owner of a depot tile.
 * @param tile Tile to get the owner of.
 * @return The depot owner.
 */
static inline Owner GetDepotOwner(TileIndex tile)
{
	if (IsRailDepotTile(tile)) {
		return GetTileOwner(GetRailDepotTile(tile));
	}
	return GetTileOwner(tile);
}

/**
 * Check if a depot belongs to a given owner.
 * @param tile The tile to check.
 * @param owner The owner to check against.
 * @return True if the depot belongs the the given owner.
 */
static inline bool IsDepotOwner(TileIndex tile, Owner o)
{
	return GetDepotOwner(tile) == o;
}

/**
 * Get the index of which depot is attached to the tile.
 * @param t the tile
 * @return DepotID
 */
static inline DepotID GetDepotIndex(const Tile *t)
{
	return t->m2;
}

/**
 * Get the index of which depot is attached to the tile.
 * @param t the tile
 * @pre IsRailDepotTile(t) || IsRoadDepotTile(t) || IsShipDepotTile(t)
 * @return DepotID
 */
static inline DepotID GetDepotIndex(TileIndex t)
{
	if (IsRailDepotTile(t)) {
		return GetDepotIndex(GetRailDepotTile(t));
	} else {
		/* Hangars don't have a Depot class, thus store no DepotID. */
		assert(IsRailDepotTile(t) || IsRoadDepotTile(t) || IsShipDepotTile(t));
		return _m[t].m2;
	}
}

/**
 * Get the type of vehicles that can use a depot
 * @param t The tile
 * @pre IsDepotTile(t)
 * @return the type of vehicles that can use the depot
 */
static inline VehicleType GetDepotVehicleType(TileIndex t)
{
	switch (GetTileType(t)) {
		default: NOT_REACHED();
		case MP_RAILWAY: return VEH_TRAIN;
		case MP_ROAD:    return VEH_ROAD;
		case MP_WATER:   return VEH_SHIP;
		case MP_STATION: return VEH_AIRCRAFT;
	}
}

#endif /* DEPOT_MAP_H */
