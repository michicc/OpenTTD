/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file road_map.h Map accessors for roads. */

#ifndef ROAD_MAP_H
#define ROAD_MAP_H

#include "track_func.h"
#include "depot_type.h"
#include "rail_type.h"
#include "road_func.h"
#include "tile_map.h"


/** Iterate through all rail tiles at a tile index. */
#define FOR_ALL_ROAD_TILES(ptr, tile) for (Tile *ptr = GetTileByType(tile, MP_ROAD); ptr != NULL; ptr = GetNextTileByType(ptr, MP_ROAD))


/** The different types of road tiles. */
enum RoadTileType {
	ROAD_TILE_NORMAL = 0, ///< Normal road
	ROAD_TILE_DEPOT  = 2, ///< Depot (one entrance)
};


static inline Tile *GetRoadTileByType(TileIndex tile, RoadType rt);

/**
 * Check if a tile pointer points to a valid road tile.
 * @param t Tile pointer to check.
 * @return True if the tile points to a road tile.
 */
static inline bool IsRoadTile(const Tile *t)
{
	return t != NULL && IsTileType(t, MP_ROAD);
}

/**
 * Get the type of the road tile.
 * @param t Tile to query.
 * @pre IsTileType(t, MP_ROAD)
 * @return The road tile type.
 */
static inline RoadTileType GetRoadTileType(const Tile *t)
{
	assert(IsTileType(t, MP_ROAD));
	return (RoadTileType)GB(t->m5, 6, 2);
}

/**
 * Return whether a tile is a normal road.
 * @param t Tile to query.
 * @pre IsTileType(t, MP_ROAD)
 * @return True if normal road.
 */
static inline bool IsNormalRoad(const Tile *t)
{
	return GetRoadTileType(t) == ROAD_TILE_NORMAL;
}

/**
 * Return whether a tile is a normal road tile.
 * @param t Tile to query.
 * @return True if normal road tile.
 */
static inline bool IsNormalRoadTile(const Tile *t)
{
	return IsRoadTile(t) && IsNormalRoad(t);
}

/**
 * Return whether a tile is a normal road tile.
 * @param t Tile to query.
 * @return True if normal road tile.
 */
static inline bool IsNormalRoadTile(TileIndex t)
{
	return IsNormalRoadTile(GetTileByType(t, MP_ROAD));
}

/**
 * Return whether a tile is a road depot.
 * @param t Tile to query.
 * @pre IsTileType(t, MP_ROAD)
 * @return True if road depot.
 */
static inline bool IsRoadDepot(const Tile *t)
{
	return GetRoadTileType(t) == ROAD_TILE_DEPOT;
}

/**
 * Return whether a tile is a road depot tile.
 * @param t Tile to query.
 * @return True if road depot tile.
 */
static inline bool IsRoadDepotTile(const Tile *t)
{
	return IsRoadTile(t) && IsRoadDepot(t);
}

/**
 * Return whether a tile is a road depot tile.
 * @param t Tile to query.
 * @return True if road depot tile.
 */
static inline bool IsRoadDepotTile(TileIndex t)
{
	return IsRoadDepotTile(GetTileByType(t, MP_ROAD));
}

/**
 * Get the actual tile for a road depot.
 * @param t Tile index to get the depot tile for.
 * @pre IsRoadDepotTile()
 * @return Pointer to the actual road depot tile.
 */
static inline Tile *GetRoadDepotTile(TileIndex t)
{
	assert(IsRoadDepotTile(t));
	return GetTileByType(t, MP_ROAD);
}

/**
 * Get the present road bits.
 * @param t  The tile to query.
 * @pre IsNormalRoad(t)
 * @return The present road bits for the road type.
 */
static inline RoadBits GetRoadBits(const Tile *t)
{
	assert(IsNormalRoad(t));
	return (RoadBits)GB(t->m5, 0, 4);
}

static inline RoadBits GetRoadBits(TileIndex t, RoadType rt)
{
	const Tile *road = GetRoadTileByType(t, rt);
	return road != NULL ? GetRoadBits(road) : ROAD_NONE;
}

/**
 * Get all RoadBits set on a tile except from the given RoadType
 *
 * @param t The tile from which we want to get the RoadBits
 * @param rt The RoadType which we exclude from the query
 * @return all set RoadBits of the tile which are not from the given RoadType
 */
static inline RoadBits GetOtherRoadBits(TileIndex t, RoadType rt)
{
	return GetRoadBits(t, rt == ROADTYPE_ROAD ? ROADTYPE_TRAM : ROADTYPE_ROAD);
}

/**
 * Get all set RoadBits on the given tile
 *
 * @param tile The tile from which we want to get the RoadBits
 * @return all set RoadBits of the tile
 */
static inline RoadBits GetAllRoadBits(TileIndex tile)
{
	RoadBits rbs = ROAD_NONE;
	FOR_ALL_ROAD_TILES(road_tile, tile) {
		rbs |= GetRoadBits(road_tile);
	}
	return rbs;
}

/**
 * Set the present road bits for a specific road type.
 * @param t  The tile to change.
 * @param r  The new road bits.
 * @pre IsNormalRoad(t)
 */
static inline void SetRoadBits(Tile *t, RoadBits r)
{
	assert(IsNormalRoad(t)); // XXX incomplete
	SB(t->m5, 0, 4, r);
}

/**
 * Get the present road types of a tile.
 * @param t The tile to query.
 * @return Present road types.
 */
static inline RoadTypes GetRoadTypes(const Tile *t)
{
	return (RoadTypes)GB(t->m7, 6, 2);
}

/**
 * Get the present road types of a tile.
 * @param tile Tile index to query.
 * @return Present road types.
 */
static inline RoadTypes GetRoadTypes(TileIndex tile)
{
	assert(IsTileType(tile, MP_TUNNELBRIDGE));
	return GetRoadTypes(_m.ToTile(tile));
}

/**
 * Get all road types present at a tile index.
 * @param tile The tile to query.
 * @return Union of all present road types.
 */
RoadTypes GetAllRoadTypes(TileIndex tile);

/**
 * Set the present road types of a tile.
 * @param t  The tile to change.
 * @param rt The new road types.
 */
static inline void SetRoadTypes(Tile *t, RoadTypes rt)
{
	assert(IsTileType(t, MP_ROAD) || IsTileType(t, MP_TUNNELBRIDGE));
	SB(t->m7, 6, 2, rt);
}

/**
 * Set the present road types of a tile.
 * @param t  The tile to change.
 * @param rt The new road types.
 */
static inline void SetRoadTypes(TileIndex tile, RoadTypes rt)
{
	SetRoadTypes(_m.ToTile(tile), rt);
}

/**
 * Check if a tile has a specific road type.
 * @param t  The tile to check.
 * @param rt Road type to check.
 * @return True if the tile has the specified road type.
 */
static inline bool HasTileRoadType(const Tile *t, RoadType rt)
{
	return HasBit(GetRoadTypes(t), rt);
}

/**
 * Get a road tile with a specific road type.
 * @param tile Tile index.
 * @param rt   Road type to search for.
 * @return Pointer to the road tile having the road type or NULL if no such tile exists.
 */
static inline Tile *GetRoadTileByType(TileIndex tile, RoadType rt)
{
	FOR_ALL_ROAD_TILES(road, tile) {
		if (HasTileRoadType(road, rt)) return road;
	}
	return NULL;
}

/**
 * Get the owner of a specific road type.
 * @param t  The tile to query.
 * @param rt The road type to get the owner of.
 * @return Owner of the given road type.
 */
static inline Owner GetRoadOwner(const Tile *t, RoadType rt)
{
	assert(IsTileType(t, MP_ROAD) || IsTileType(t, MP_STATION) || IsTileType(t, MP_TUNNELBRIDGE));
	switch (rt) {
		default: NOT_REACHED();
		case ROADTYPE_ROAD: return (Owner)GB(IsNormalRoadTile(t) ? t->m1 : t->m7, 0, 5);
		case ROADTYPE_TRAM: {
			/* Trams don't need OWNER_TOWN, and remapping OWNER_NONE
			 * to OWNER_TOWN makes it use one bit less */
			Owner o = (Owner)GB(t->m3, 4, 4);
			return o == OWNER_TOWN ? OWNER_NONE : o;
		}
	}
}

/**
 * Get the owner of a specific road type.
 * @param t  The tile to query.
 * @param rt The road type to get the owner of.
 * @return Owner of the given road type.
 */
static inline Owner GetRoadOwner(TileIndex t, RoadType rt)
{
	assert(IsTileType(t, MP_TUNNELBRIDGE));
	return GetRoadOwner(_m.ToTile(t), rt);
}

/**
 * Set the owner of a specific road type.
 * @param t  The tile to change.
 * @param rt The road type to change the owner of.
 * @param o  New owner of the given road type.
 */
static inline void SetRoadOwner(Tile *t, RoadType rt, Owner o)
{
	switch (rt) {
		default: NOT_REACHED();
		case ROADTYPE_ROAD: SB(IsNormalRoadTile(t) ? t->m1 : t->m7, 0, 5, o); break;
		case ROADTYPE_TRAM: SB(t->m3, 4, 4, o == OWNER_NONE ? OWNER_TOWN : o); break;
	}
}

/**
 * Set the owner of a specific road type.
 * @param t  The tile to change.
 * @param rt The road type to change the owner of.
 * @param o  New owner of the given road type.
 */
static inline void SetRoadOwner(TileIndex t, RoadType rt, Owner o)
{
	assert(IsTileType(t, MP_TUNNELBRIDGE));
	SetRoadOwner(_m.ToTile(t), rt, o);
}

/**
 * Checks if given tile has town owned road
 * @param t tile to check
 * @return true iff tile has road and the road is owned by a town
 */
static inline bool HasTownOwnedRoad(TileIndex t)
{
	const Tile *road = GetRoadTileByType(t, ROADTYPE_ROAD);
	return road != NULL && IsTileOwner(road, OWNER_TOWN);
}

/** Which directions are disallowed ? */
enum DisallowedRoadDirections {
	DRD_NONE,       ///< None of the directions are disallowed
	DRD_SOUTHBOUND, ///< All southbound traffic is disallowed
	DRD_NORTHBOUND, ///< All northbound traffic is disallowed
	DRD_BOTH,       ///< All directions are disallowed
	DRD_END,        ///< Sentinel
};
DECLARE_ENUM_AS_BIT_SET(DisallowedRoadDirections)
/** Helper information for extract tool. */
template <> struct EnumPropsT<DisallowedRoadDirections> : MakeEnumPropsT<DisallowedRoadDirections, byte, DRD_NONE, DRD_END, DRD_END, 2> {};

/**
 * Gets the disallowed directions
 * @param t the tile to get the directions from
 * @return the disallowed directions
 */
static inline DisallowedRoadDirections GetDisallowedRoadDirections(const Tile *t)
{
	assert(IsNormalRoad(t));
	return (DisallowedRoadDirections)GB(t->m5, 4, 2);
}

/**
 * Sets the disallowed directions
 * @param t   the tile to set the directions for
 * @param drd the disallowed directions
 */
static inline void SetDisallowedRoadDirections(Tile *t, DisallowedRoadDirections drd)
{
	assert(IsNormalRoad(t));
	assert(drd < DRD_END);
	SB(t->m5, 4, 2, drd);
}


/** The possible road side decorations. */
enum Roadside {
	ROADSIDE_NONE             = 0, ///< No road side
	ROADSIDE_GRASS            = 1, ///< Road on grass
	ROADSIDE_PAVED            = 2, ///< Road with paved sidewalks
	ROADSIDE_STREET_LIGHTS    = 3, ///< Road with street lights on paved sidewalks
	// 4 is unused for historical reasons
	ROADSIDE_TREES            = 5, ///< Road with trees on paved sidewalks
	ROADSIDE_GRASS_ROAD_WORKS = 6, ///< Road on grass with road works
	ROADSIDE_PAVED_ROAD_WORKS = 7, ///< Road with sidewalks and road works
};

/**
 * Get the decorations of a road.
 * @param tile The tile to query.
 * @return The road decoration of the tile.
 */
static inline Roadside GetRoadside(const Tile *tile)
{
	return (Roadside)GB(tile->m6, 3, 3);
}

/**
 * Set the decorations of a road.
 * @param tile The tile to change.
 * @param s    The new road decoration of the tile.
 */
static inline void SetRoadside(Tile *tile, Roadside s)
{
	SB(tile->m6, 3, 3, s);
}

/**
 * Check if a tile has road works.
 * @param t The tile to check.
 * @return True if the tile has road works in progress.
 */
static inline bool HasRoadWorks(const Tile *t)
{
	return GetRoadside(t) >= ROADSIDE_GRASS_ROAD_WORKS;
}

/**
 * Check if a tile has road works.
 * @param t The tile to check.
 * @return True if the tile has road works in progress.
 */
static inline bool HasRoadWorks(TileIndex t)
{
	FOR_ALL_ROAD_TILES(road, t) {
		if (HasRoadWorks(road)) return true;
	}
	return false;
}

/**
 * Increase the progress counter of road works.
 * @param t The tile to modify.
 * @return True if the road works are in the last stage.
 */
static inline bool IncreaseRoadWorksCounter(Tile *t)
{
	AB(t->m7, 0, 4, 1);

	return GB(t->m7, 0, 4) == 15;
}

/**
 * Start road works on a tile.
 * @param t The tile to start the work on.
 * @pre !HasRoadWorks(t)
 */
static inline void StartRoadWorks(Tile *t)
{
	assert(!HasRoadWorks(t));
	/* Remove any trees or lamps in case or roadwork */
	switch (GetRoadside(t)) {
		case ROADSIDE_NONE:
		case ROADSIDE_GRASS:  SetRoadside(t, ROADSIDE_GRASS_ROAD_WORKS); break;
		default:              SetRoadside(t, ROADSIDE_PAVED_ROAD_WORKS); break;
	}
}

/**
 * Terminate road works on a tile.
 * @param t Tile to stop the road works on.
 * @pre HasRoadWorks(t)
 */
static inline void TerminateRoadWorks(Tile *t)
{
	assert(HasRoadWorks(t));
	SetRoadside(t, (Roadside)(GetRoadside(t) - ROADSIDE_GRASS_ROAD_WORKS + ROADSIDE_GRASS));
	/* Stop the counter */
	SB(t->m7, 0, 4, 0);
}


/**
 * Get the direction of the exit of a road depot.
 * @param t The tile to query.
 * @return Diagonal direction of the depot exit.
 */
static inline DiagDirection GetRoadDepotDirection(const Tile *t)
{
	assert(IsRoadDepot(t));
	return (DiagDirection)GB(t->m5, 0, 2);
}


RoadBits GetAnyRoadBits(TileIndex tile, RoadType rt, bool straight_tunnel_bridge_entrance = false);


/**
 * Make a normal road tile.
 * @param road_tile Tile to make a normal road.
 * @param bits Road bits to set for all present road types.
 * @param rt   New road type.
 * @param town Town ID if the road is a town-owned road.
 * @param road New owner of road.
 * @param tram New owner of tram tracks.
 * @return Pointer to the new road tile.
 */
static inline Tile *MakeRoadNormal(Tile *road_tile, RoadBits bits, RoadType rt, TownID town, Owner o)
{
	SetTileOwner(road_tile, o);
	road_tile->m2 = town;
	road_tile->m3 = bits;
	road_tile->m5 = bits | ROAD_TILE_NORMAL << 6;
	road_tile->m7 = RoadTypeToRoadTypes(rt) << 6;
	return road_tile;
}

/**
 * Make a normal road tile.
 * @param t    Tile to make a normal road.
 * @param bits Road bits to set for all present road types.
 * @param rt   New road type.
 * @param town Town ID if the road is a town-owned road.
 * @param road New owner of road.
 * @param tram New owner of tram tracks.
 * @return Pointer to the new road tile.
 */
static inline Tile *MakeRoadNormal(TileIndex t, RoadBits bits, RoadType rt, TownID town, Owner o)
{
	/* Insert ROADTYPE_ROAD in front, all other types at the back, but before a possible station tile. */
	Tile *st_tile = GetTileByType(t, MP_STATION);
	Tile *road_tile = _m.NewTile(t, MP_ROAD, false, rt == ROADTYPE_ROAD ? _m.ToTile(t) : (st_tile != NULL ? st_tile - 1 : NULL));
	return MakeRoadNormal(road_tile, bits, rt, town, o);
}

/**
 * Make a road depot.
 * @param t     Tile to make a road depot.
 * @param owner New owner of the depot.
 * @param did   New depot ID.
 * @param dir   Direction of the depot exit.
 * @param rt    Road type of the depot.
 */
static inline void MakeRoadDepot(TileIndex t, Owner owner, DepotID did, DiagDirection dir, RoadType rt)
{
	Tile *road_tile = _m.NewTile(t, MP_ROAD);
	SetTileOwner(road_tile, owner);
	road_tile->m2 = did;
	road_tile->m5 = ROAD_TILE_DEPOT << 6 | dir;
	road_tile->m7 = RoadTypeToRoadTypes(rt) << 6 | owner;
}

#endif /* ROAD_MAP_H */
