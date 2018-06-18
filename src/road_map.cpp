/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file road_map.cpp Complex road accessors. */

#include "stdafx.h"
#include "station_map.h"
#include "tunnelbridge_map.h"

#include "safeguards.h"


/**
 * Returns the RoadBits on an arbitrary tile
 * Special behaviour:
 * - road depots: entrance is treated as road piece
 * - road tunnels: entrance is treated as road piece
 * - bridge ramps: start of the ramp is treated as road piece
 * - bridge middle parts: bridge itself is ignored
 *
 * If straight_tunnel_bridge_entrance is set a ROAD_X or ROAD_Y
 * for bridge ramps and tunnel entrances is returned depending
 * on the orientation of the tunnel or bridge.
 * @param tile the tile to get the road bits for
 * @param rt   the road type to get the road bits form
 * @param straight_tunnel_bridge_entrance whether to return straight road bits for tunnels/bridges.
 * @return the road bits of the given tile
 */
RoadBits GetAnyRoadBits(TileIndex tile, RoadType rt, bool straight_tunnel_bridge_entrance)
{
	const Tile *road_tile = GetRoadTileByType(tile, rt);
	if (road_tile != NULL) {
		switch (GetRoadTileType(road_tile)) {
			default:
			case ROAD_TILE_NORMAL:   return GetRoadBits(road_tile);
			case ROAD_TILE_DEPOT:    return DiagDirToRoadBits(GetRoadDepotDirection(road_tile));
		}
	}

	if (IsTileType(tile, MP_TUNNELBRIDGE)) {
		if (GetTunnelBridgeTransportType(tile) != TRANSPORT_ROAD) return ROAD_NONE;
		return straight_tunnel_bridge_entrance ?
				AxisToRoadBits(DiagDirToAxis(GetTunnelBridgeDirection(tile))) :
				DiagDirToRoadBits(ReverseDiagDir(GetTunnelBridgeDirection(tile)));
	}

	return ROAD_NONE;
}

RoadTypes GetAllRoadTypes(TileIndex tile)
{
	if (HasTileByType(tile, MP_ROAD)) {
		RoadTypes rts = ROADTYPES_NONE;
		FOR_ALL_ROAD_TILES(road, tile) {
			rts |= GetRoadTypes(road);
		}
		return rts;
	}

	if (IsTileType(tile, MP_TUNNELBRIDGE)) {
		if (GetTunnelBridgeTransportType(tile) != TRANSPORT_ROAD) return ROADTYPES_NONE;
		return GetRoadTypes(_m.ToTile(tile));
	}

	return ROADTYPES_NONE;
}
