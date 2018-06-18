/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file road_cmd.cpp Commands related to road tiles. */

#include "stdafx.h"
#include "cmd_helper.h"
#include "road_internal.h"
#include "viewport_func.h"
#include "command_func.h"
#include "pathfinder/yapf/yapf_cache.h"
#include "depot_base.h"
#include "newgrf.h"
#include "autoslope.h"
#include "tunnelbridge_map.h"
#include "strings_func.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "tunnelbridge.h"
#include "cheat_type.h"
#include "effectvehicle_func.h"
#include "effectvehicle_base.h"
#include "elrail_func.h"
#include "roadveh.h"
#include "town.h"
#include "company_base.h"
#include "core/random_func.hpp"
#include "newgrf_railtype.h"
#include "date_func.h"
#include "genworld.h"
#include "company_gui.h"
#include "clear_map.h"

#include "table/strings.h"

#include "safeguards.h"

/**
 * Verify whether a road vehicle is available.
 * @return \c true if at least one road vehicle is available, \c false if not
 */
bool RoadVehiclesAreBuilt()
{
	const RoadVehicle *rv;
	FOR_ALL_ROADVEHICLES(rv) return true;

	return false;
}

/** Invalid RoadBits on slopes.  */
static const RoadBits _invalid_tileh_slopes_road[2][15] = {
	/* The inverse of the mixable RoadBits on a leveled slope */
	{
		ROAD_NONE,         // SLOPE_FLAT
		ROAD_NE | ROAD_SE, // SLOPE_W
		ROAD_NE | ROAD_NW, // SLOPE_S

		ROAD_NE,           // SLOPE_SW
		ROAD_NW | ROAD_SW, // SLOPE_E
		ROAD_NONE,         // SLOPE_EW

		ROAD_NW,           // SLOPE_SE
		ROAD_NONE,         // SLOPE_WSE
		ROAD_SE | ROAD_SW, // SLOPE_N

		ROAD_SE,           // SLOPE_NW
		ROAD_NONE,         // SLOPE_NS
		ROAD_NONE,         // SLOPE_ENW

		ROAD_SW,           // SLOPE_NE
		ROAD_NONE,         // SLOPE_SEN
		ROAD_NONE          // SLOPE_NWS
	},
	/* The inverse of the allowed straight roads on a slope
	 * (with and without a foundation). */
	{
		ROAD_NONE, // SLOPE_FLAT
		ROAD_NONE, // SLOPE_W    Foundation
		ROAD_NONE, // SLOPE_S    Foundation

		ROAD_Y,    // SLOPE_SW
		ROAD_NONE, // SLOPE_E    Foundation
		ROAD_ALL,  // SLOPE_EW

		ROAD_X,    // SLOPE_SE
		ROAD_ALL,  // SLOPE_WSE
		ROAD_NONE, // SLOPE_N    Foundation

		ROAD_X,    // SLOPE_NW
		ROAD_ALL,  // SLOPE_NS
		ROAD_ALL,  // SLOPE_ENW

		ROAD_Y,    // SLOPE_NE
		ROAD_ALL,  // SLOPE_SEN
		ROAD_ALL   // SLOPE_NW
	}
};

static Foundation GetRoadFoundation(Slope tileh, RoadBits bits);

/**
 * Is it allowed to remove the given road bits from the given tile?
 * @param tile      the tile to remove the road from
 * @param remove    the roadbits that are going to be removed
 * @param owner     the actual owner of the roadbits of the tile
 * @param rt        the road type to remove the bits from
 * @param flags     command flags
 * @param town_check Shall the town rating checked/affected
 * @return A succeeded command when it is allowed to remove the road bits, a failed command otherwise.
 */
CommandCost CheckAllowRemoveRoad(TileIndex tile, RoadBits remove, Owner owner, RoadType rt, DoCommandFlag flags, bool town_check)
{
	if (_game_mode == GM_EDITOR || remove == ROAD_NONE) return CommandCost();

	/* Water can always flood and towns can always remove "normal" road pieces.
	 * Towns are not be allowed to remove non "normal" road pieces, like tram
	 * tracks as that would result in trams that cannot turn. */
	if (_current_company == OWNER_WATER ||
			(rt == ROADTYPE_ROAD && !Company::IsValidID(_current_company))) return CommandCost();

	/* Only do the special processing if the road is owned
	 * by a town */
	if (owner != OWNER_TOWN) {
		if (owner == OWNER_NONE) return CommandCost();
		CommandCost ret = CheckOwnership(owner);
		return ret;
	}

	if (!town_check) return CommandCost();

	if (_cheats.magic_bulldozer.value) return CommandCost();

	Town *t = ClosestTownFromTile(tile, UINT_MAX);
	if (t == NULL) return CommandCost();

	/* check if you're allowed to remove the street owned by a town
	 * removal allowance depends on difficulty setting */
	CommandCost ret = CheckforTownRating(flags, t, ROAD_REMOVE);
	if (ret.Failed()) return ret;

	/* Get a bitmask of which neighbouring roads has a tile */
	RoadBits n = ROAD_NONE;
	RoadBits present = GetAnyRoadBits(tile, rt);
	if ((present & ROAD_NE) && (GetAnyRoadBits(TILE_ADDXY(tile, -1,  0), rt) & ROAD_SW)) n |= ROAD_NE;
	if ((present & ROAD_SE) && (GetAnyRoadBits(TILE_ADDXY(tile,  0,  1), rt) & ROAD_NW)) n |= ROAD_SE;
	if ((present & ROAD_SW) && (GetAnyRoadBits(TILE_ADDXY(tile,  1,  0), rt) & ROAD_NE)) n |= ROAD_SW;
	if ((present & ROAD_NW) && (GetAnyRoadBits(TILE_ADDXY(tile,  0, -1), rt) & ROAD_SE)) n |= ROAD_NW;

	int rating_decrease = RATING_ROAD_DOWN_STEP_EDGE;
	/* If 0 or 1 bits are set in n, or if no bits that match the bits to remove,
	 * then allow it */
	if (KillFirstBit(n) != ROAD_NONE && (n & remove) != ROAD_NONE) {
		/* you can remove all kind of roads with extra dynamite */
		if (!_settings_game.construction.extra_dynamite) {
			SetDParam(0, t->index);
			return_cmd_error(STR_ERROR_LOCAL_AUTHORITY_REFUSES_TO_ALLOW_THIS);
		}
		rating_decrease = RATING_ROAD_DOWN_STEP_INNER;
	}
	ChangeTownRating(t, rating_decrease, RATING_ROAD_MINIMUM, flags);

	return CommandCost();
}

/**
 * Delete a piece of road from a MP_ROAD tile.
 * @param tile Tile to remove road from.
 * @param road_tile Pointer to the actual road tile.
 * @param flags Operation to perform.
 * @param pieces Road bits to remove.
 * @param rt Road type to remove.
 * @param crossing_check Should we check if there is a tram track when we are removing road from crossing?
 * @param town_check Should we check if the town allows removal?
 * @param[out] tile_deleted Was #road_tile removed from the map array?
 * @return Cost of removing or error if the road can't be removed.
 */
static CommandCost RemoveRoadReal(TileIndex tile, Tile *road_tile, DoCommandFlag flags, RoadBits pieces, RoadType rt, bool crossing_check, bool town_check = true, bool *tile_deleted = NULL)
{
	/* Check for normal road tile. */
	if (!IsNormalRoadTile(road_tile) || !HasTileRoadType(road_tile, rt)) return CMD_ERROR;

	CommandCost ret = EnsureNoVehicleOnGround(tile);
	if (ret.Failed()) return ret;

	Owner road_owner = GetTileOwner(road_tile);
	ret = CheckAllowRemoveRoad(tile, pieces, road_owner, rt, flags, town_check);
	if (ret.Failed()) return ret;

	/* Steep slopes behave the same as slopes with one corner raised. */
	Slope tileh = GetTileSlope(tile);
	if (IsSteepSlope(tileh)) tileh = SlopeWithOneCornerRaised(GetHighestSlopeCorner(tileh));

	RoadBits present = GetRoadBits(road_tile);
	const RoadBits other = GetOtherRoadBits(tile, rt);
	const Foundation f = GetRoadFoundation(tileh, present);

	if (HasRoadWorks(tile) && _current_company != OWNER_WATER) return_cmd_error(STR_ERROR_ROAD_WORKS_IN_PROGRESS);

	/* Autocomplete to a straight road
	 * @li if the bits of the other roadtypes result in another foundation
	 * @li if build on slopes is disabled
	 * @li if the tile is a level crossing */
	if ((IsStraightRoad(other) && (other & _invalid_tileh_slopes_road[0][tileh & SLOPE_ELEVATED]) != ROAD_NONE) ||
			(tileh != SLOPE_FLAT && !_settings_game.construction.build_on_slopes) || IsLevelCrossingTile(tile)) {
		pieces |= MirrorRoadBits(pieces);
	}

	/* Don't allow road to be removed from the crossing when there is tram;
	 * we can't draw the crossing without roadbits ;) */
	if (crossing_check && IsLevelCrossingTile(tile) && rt == ROADTYPE_ROAD && HasTileRoadType(road_tile, ROADTYPE_TRAM)) return CMD_ERROR;

	/* limit the bits to delete to the existing bits. */
	pieces &= present;
	if (pieces == ROAD_NONE) return_cmd_error(rt == ROADTYPE_TRAM ? STR_ERROR_THERE_IS_NO_TRAMWAY : STR_ERROR_THERE_IS_NO_ROAD);

	/* Now set present what it will be after the remove */
	present ^= pieces;

	/* Check for invalid RoadBit combinations on slopes */
	if (tileh != SLOPE_FLAT && present != ROAD_NONE &&
			(present & _invalid_tileh_slopes_road[0][tileh & SLOPE_ELEVATED]) == present) {
		return CMD_ERROR;
	}

	if (flags & DC_EXEC) {
		if (HasRoadWorks(road_tile)) {
			/* flooding tile with road works, don't forget to remove the effect vehicle too */
			assert(_current_company == OWNER_WATER);
			EffectVehicle *v;
			FOR_ALL_EFFECTVEHICLES(v) {
				if (TileVirtXY(v->x_pos, v->y_pos) == tile) {
					delete v;
				}
			}
		}

		/* Update infrastructure counts. */
		Company *c = Company::GetIfValid(GetTileOwner(road_tile));
		if (c != NULL) {
			c->infrastructure.road[rt] -= CountBits(pieces);
			DirtyCompanyInfrastructureWindows(c->index);
		}

		if (present == ROAD_NONE) {
			/* No more road bits left, delete associated tile. */
			bool town_road = rt == ROADTYPE_ROAD && IsTileOwner(road_tile, OWNER_TOWN);
			Roadside rs = GetRoadside(road_tile);

			_m.RemoveTile(tile, road_tile);
			if (tile_deleted != NULL) *tile_deleted = true;

			if (HasTileByType(tile, MP_ROAD)) {
				/* Still some road tiles left. */
				if (town_road) {
					/* Update nearest-town index */
					const Town *town = CalcClosestTownFromTile(tile);
					FOR_ALL_ROAD_TILES(t, tile) {
						SetTownIndex(t, town == NULL ? (TownID)INVALID_TOWN : town->index);
					}
				}
				/* Road side is determined by the first road tile. If we just
				 * deleted that, we need to propagate the road side. */
				if (rs != ROADSIDE_NONE) SetRoadside(GetTileByType(tile, MP_ROAD), rs);
			} else {
				if (IsLevelCrossingTile(tile)) {
					/* Remove crossing if this was the last road tile. */
					Tile *crossing = GetLevelCrossingTile(tile);
					UnbarCrossing(crossing);
					SetLevelCrossing(crossing, false);
					c = Company::GetIfValid(GetTileOwner(crossing));
					/* Subtract count for a level crossing and add count for a single straight rail piece instead. */
					if (c != NULL) {
						c->infrastructure.rail[GetRailType(crossing)] -= LEVELCROSSING_TRACKBIT_FACTOR - 1;
						DirtyCompanyInfrastructureWindows(c->index);
					}
					YapfNotifyTrackLayoutChange(tile, INVALID_TRACK);
				}
				MakeClearGrass(tile);
			}
			MarkTileDirtyByTile(tile);
		} else {
			/* When bits are removed, you *always* end up with something that
			 * is not a complete straight road tile. However, trams do not have
			 * onewayness, so they cannot remove it either. */
			if (rt != ROADTYPE_TRAM) SetDisallowedRoadDirections(road_tile, DRD_NONE);
			SetRoadBits(road_tile, present);
			MarkTileDirtyByTile(tile);
		}
	}

	CommandCost cost(EXPENSES_CONSTRUCTION, CountBits(pieces) * _price[PR_CLEAR_ROAD]);
	/* If we build a foundation we have to pay for it. */
	if (f == FOUNDATION_NONE && GetRoadFoundation(tileh, present) != FOUNDATION_NONE) cost.AddCost(_price[PR_BUILD_FOUNDATION]);

	return cost;
}

/**
 * Delete a piece of road.
 * @param tile tile where to remove road from
 * @param flags operation to perform
 * @param pieces roadbits to remove
 * @param rt roadtype to remove
 * @param crossing_check should we check if there is a tram track when we are removing road from crossing?
 * @param town_check should we check if the town allows removal?
 */
static CommandCost RemoveRoad(TileIndex tile, DoCommandFlag flags, RoadBits pieces, RoadType rt, bool crossing_check, bool town_check = true)
{
	RoadTypes rts = GetAllRoadTypes(tile);
	/* The tile doesn't have the given road type */
	if (!HasBit(rts, rt)) return_cmd_error(rt == ROADTYPE_TRAM ? STR_ERROR_THERE_IS_NO_TRAMWAY : STR_ERROR_THERE_IS_NO_ROAD);

	if (HasTileByType(tile, MP_STATION) && !IsDriveThroughStopTile(tile)) return CMD_ERROR;
	if (HasTileByType(tile, MP_ROAD)) {
		Tile *road_tile = GetRoadTileByType(tile, rt);
		return RemoveRoadReal(tile, road_tile, flags, pieces, rt, crossing_check, town_check);
	}

	if (GetTileType(tile) != MP_TUNNELBRIDGE) return CMD_ERROR;

	if (GetTunnelBridgeTransportType(tile) != TRANSPORT_ROAD) return CMD_ERROR;
	CommandCost ret = TunnelBridgeIsFree(tile, GetOtherTunnelBridgeEnd(tile));
	if (ret.Failed()) return ret;

	Owner road_owner = GetRoadOwner(tile, rt);
	ret = CheckAllowRemoveRoad(tile, pieces, road_owner, rt, flags, town_check);
	if (ret.Failed()) return ret;

	/* If it's the last roadtype, just clear the whole tile */
	if (rts == RoadTypeToRoadTypes(rt)) return DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);

	CommandCost cost(EXPENSES_CONSTRUCTION);
	TileIndex other_end = GetOtherTunnelBridgeEnd(tile);
	/* Pay for *every* tile of the bridge or tunnel */
	uint len = GetTunnelBridgeLength(other_end, tile) + 2;
	cost.AddCost(len * 2 * _price[PR_CLEAR_ROAD]);
	if (flags & DC_EXEC) {
		Company *c = Company::GetIfValid(GetRoadOwner(tile, rt));
		if (c != NULL) {
			/* A full diagonal road tile has two road bits. */
			c->infrastructure.road[rt] -= len * 2 * TUNNELBRIDGE_TRACKBIT_FACTOR;
			DirtyCompanyInfrastructureWindows(c->index);
		}

		SetRoadTypes(other_end, GetRoadTypes(other_end) & ~RoadTypeToRoadTypes(rt));
		SetRoadTypes(tile, GetRoadTypes(tile) & ~RoadTypeToRoadTypes(rt));

		/* If the owner of the bridge sells all its road, also move the ownership
		* to the owner of the other roadtype, unless the bridge owner is a town. */
		RoadType other_rt = (rt == ROADTYPE_ROAD) ? ROADTYPE_TRAM : ROADTYPE_ROAD;
		Owner other_owner = GetRoadOwner(tile, other_rt);
		if (!IsTileOwner(tile, other_owner) && !IsTileOwner(tile, OWNER_TOWN)) {
			SetTileOwner(tile, other_owner);
			SetTileOwner(other_end, other_owner);
		}

		/* Mark tiles dirty that have been repaved. */
		if (IsBridge(tile)) {
			MarkBridgeDirty(tile);
		} else {
			MarkTileDirtyByTile(tile);
			MarkTileDirtyByTile(other_end);
		}
	}
	return cost;
}


/**
 * Calculate the costs for roads on slopes
 *  Aside modify the RoadBits to fit on the slopes
 *
 * @note The RoadBits are modified too!
 * @param tileh The current slope
 * @param pieces The RoadBits we want to add
 * @param existing The existent RoadBits of the current type
 * @param other The other existent RoadBits
 * @return The costs for these RoadBits on this slope
 */
static CommandCost CheckRoadSlope(Slope tileh, RoadBits *pieces, RoadBits existing, RoadBits other)
{
	/* Remove already build pieces */
	CLRBITS(*pieces, existing);

	/* If we can't build anything stop here */
	if (*pieces == ROAD_NONE) return CMD_ERROR;

	/* All RoadBit combos are valid on flat land */
	if (tileh == SLOPE_FLAT) return CommandCost();

	/* Steep slopes behave the same as slopes with one corner raised. */
	if (IsSteepSlope(tileh)) {
		tileh = SlopeWithOneCornerRaised(GetHighestSlopeCorner(tileh));
	}

	/* Save the merge of all bits of the current type */
	RoadBits type_bits = existing | *pieces;

	/* Roads on slopes */
	if (_settings_game.construction.build_on_slopes && (_invalid_tileh_slopes_road[0][tileh] & (other | type_bits)) == ROAD_NONE) {

		/* If we add leveling we've got to pay for it */
		if ((other | existing) == ROAD_NONE) return CommandCost(EXPENSES_CONSTRUCTION, _price[PR_BUILD_FOUNDATION]);

		return CommandCost();
	}

	/* Autocomplete uphill roads */
	*pieces |= MirrorRoadBits(*pieces);
	type_bits = existing | *pieces;

	/* Uphill roads */
	if (IsStraightRoad(type_bits) && (other == type_bits || other == ROAD_NONE) &&
			(_invalid_tileh_slopes_road[1][tileh] & (other | type_bits)) == ROAD_NONE) {

		/* Slopes with foundation ? */
		if (IsSlopeWithOneCornerRaised(tileh)) {

			/* Prevent build on slopes if it isn't allowed */
			if (_settings_game.construction.build_on_slopes) {

				/* If we add foundation we've got to pay for it */
				if ((other | existing) == ROAD_NONE) return CommandCost(EXPENSES_CONSTRUCTION, _price[PR_BUILD_FOUNDATION]);

				return CommandCost();
			}
		} else {
			if (HasExactlyOneBit(existing) && GetRoadFoundation(tileh, existing) == FOUNDATION_NONE) return CommandCost(EXPENSES_CONSTRUCTION, _price[PR_BUILD_FOUNDATION]);
			return CommandCost();
		}
	}
	return CMD_ERROR;
}

/**
 * Build a piece of road.
 * @param tile tile where to build road
 * @param flags operation to perform
 * @param p1 bit 0..3 road pieces to build (RoadBits)
 *           bit 4..5 road type
 *           bit 6..7 disallowed directions to toggle
 * @param p2 the town that is building the road (0 if not applicable)
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdBuildRoad(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	CompanyID company = _current_company;
	CommandCost cost(EXPENSES_CONSTRUCTION);

	RoadBits existing = ROAD_NONE;
	RoadBits other_bits = ROAD_NONE;

	/* Road pieces are max 4 bitset values (NE, NW, SE, SW) and town can only be non-zero
	 * if a non-company is building the road */
	if ((Company::IsValidID(company) && p2 != 0) || (company == OWNER_TOWN && !Town::IsValidID(p2)) || (company == OWNER_DEITY && p2 != 0)) return CMD_ERROR;
	if (company != OWNER_TOWN) {
		const Town *town = CalcClosestTownFromTile(tile);
		p2 = (town != NULL) ? town->index : INVALID_TOWN;

		if (company == OWNER_DEITY) {
			company = OWNER_TOWN;

			/* If we are not within a town, we are not owned by the town */
			if (town == NULL || DistanceSquare(tile, town->xy) > town->cache.squared_town_zone_radius[HZB_TOWN_EDGE]) {
				company = OWNER_NONE;
			}
		}
	}

	RoadBits pieces = Extract<RoadBits, 0, 4>(p1);

	/* do not allow building 'zero' road bits, code wouldn't handle it */
	if (pieces == ROAD_NONE) return CMD_ERROR;

	RoadType rt = Extract<RoadType, 4, 2>(p1);
	if (!IsValidRoadType(rt) || !ValParamRoadType(rt)) return CMD_ERROR;

	DisallowedRoadDirections toggle_drd = Extract<DisallowedRoadDirections, 6, 2>(p1);

	Slope tileh = GetTileSlope(tile);

	bool need_to_clear = false;

	if (HasTileByType(tile, MP_RAILWAY)) {
		/* Already a level crossing or going to be one. */

		if (IsSteepSlope(tileh)) {
			return_cmd_error(STR_ERROR_LAND_SLOPED_IN_WRONG_DIRECTION);
		}

		/* Level crossings may only be built on these slopes */
		if (!HasBit(VALID_LEVEL_CROSSING_SLOPES, tileh)) {
			return_cmd_error(STR_ERROR_LAND_SLOPED_IN_WRONG_DIRECTION);
		}

		FOR_ALL_RAIL_TILES(rail_tile, tile) {
			if (GetRailTileType(rail_tile) != RAIL_TILE_NORMAL && GetRailTileType(rail_tile) != RAIL_TILE_CROSSING) goto do_clear;

			if (RailNoLevelCrossings(GetRailType(rail_tile))) {
				return_cmd_error(STR_ERROR_CROSSING_DISALLOWED);
			}

			switch (GetTrackBits(rail_tile)) {
				case TRACK_BIT_X:
					if (pieces & ROAD_X) goto do_clear;
					pieces = ROAD_Y;
					break;

				case TRACK_BIT_Y:
					if (pieces & ROAD_Y) goto do_clear;
					pieces = ROAD_X;
					break;

				default: goto do_clear;
			}
		}

		if (GetRoadTileByType(tile, rt) != NULL) return_cmd_error(STR_ERROR_ALREADY_BUILT);

		CommandCost ret = EnsureNoVehicleOnGround(tile);
		if (ret.Failed()) return ret;

		if (flags & DC_EXEC) {
			Company *c = Company::GetIfValid(company);

			if (rt != ROADTYPE_ROAD && GetRoadTileByType(tile, ROADTYPE_ROAD) == NULL) {
				/* No road present. Add it as well because we can't draw a level crossing without road. */
				MakeRoadNormal(tile, pieces, ROADTYPE_ROAD, p2, company);
				if (c != NULL) c->infrastructure.road[ROADTYPE_ROAD] += 2;
			}

			MakeRoadNormal(tile, pieces, rt, p2, company);
			/* Update company infrastructure counts. A level crossing has two road bits. */
			if (c != NULL) {
				c->infrastructure.road[rt] += 2;
				DirtyCompanyInfrastructureWindows(c->index);
			}

			/* Can't have more than one rail tile for a valid level crossing. */
			Tile *crossing = GetTileByType(tile, MP_RAILWAY);
			if (!IsLevelCrossing(crossing)) {
				SetLevelCrossing(crossing, true);
				YapfNotifyTrackLayoutChange(tile, pieces == ROAD_X ? TRACK_Y : TRACK_X);
				UpdateLevelCrossing(tile, false);
				/* Update rail count for level crossings. The plain track is already
				 * counted, so only add the difference to the level crossing cost. */
				c = Company::GetIfValid(GetTileOwner(crossing));
				if (c != NULL) {
					c->infrastructure.rail[GetRailType(crossing)] += LEVELCROSSING_TRACKBIT_FACTOR - 1;
					DirtyCompanyInfrastructureWindows(c->index);
				}
			}
			MakeClearGrass(tile);
			MarkTileDirtyByTile(tile);
		}
		return CommandCost(EXPENSES_CONSTRUCTION, _price[PR_BUILD_ROAD] * (rt == ROADTYPE_ROAD ? 2 : 4));
	}

	if (HasTileByType(tile, MP_STATION)) {
		Tile *st_tile = GetTileByType(tile, MP_STATION);
		if ((GetAnyRoadBits(tile, rt) & pieces) == pieces) return_cmd_error(STR_ERROR_ALREADY_BUILT);
		if (!IsDriveThroughStop(st_tile)) goto do_clear;

		RoadBits curbits = AxisToRoadBits(DiagDirToAxis(GetRoadStopDir(st_tile)));
		if (pieces & ~curbits) goto do_clear;
		pieces = curbits; // we need to pay for both roadbits

		if (GetRoadTileByType(tile, rt) != NULL) return_cmd_error(STR_ERROR_ALREADY_BUILT);
	} else if (HasTileByType(tile, MP_ROAD)) {
		/* Check all road tiles for compatibility. */
		FOR_ALL_ROAD_TILES(road_tile, tile) {
			switch (GetRoadTileType(road_tile)) {
				case ROAD_TILE_NORMAL: {
					if (HasRoadWorks(road_tile)) return_cmd_error(STR_ERROR_ROAD_WORKS_IN_PROGRESS);

					if (!HasBit(GetRoadTypes(road_tile), rt)) {
						/* Not the current road type, save bits for later. */
						other_bits |= GetRoadBits(road_tile);
						break;
					}

					existing = GetRoadBits(road_tile);
					bool crossing = !IsStraightRoad(existing | pieces);
					if (rt == ROADTYPE_ROAD && (GetDisallowedRoadDirections(road_tile) != DRD_NONE || toggle_drd != DRD_NONE) && crossing) {
						/* Junctions cannot be one-way. */
						return_cmd_error(STR_ERROR_ONEWAY_ROADS_CAN_T_HAVE_JUNCTION);
					}

					if ((existing & pieces) == pieces) {
						/* We only want to set the (dis)allowed road directions. */
						if (toggle_drd != DRD_NONE && rt == ROADTYPE_ROAD) {
							if (crossing) return_cmd_error(STR_ERROR_ONEWAY_ROADS_CAN_T_HAVE_JUNCTION);

							Owner owner = GetTileOwner(road_tile);
							if (owner != OWNER_NONE) {
								CommandCost ret = CheckOwnership(owner, tile);
								if (ret.Failed()) return ret;
							}

							DisallowedRoadDirections dis_existing = GetDisallowedRoadDirections(road_tile);
							DisallowedRoadDirections dis_new      = dis_existing ^ toggle_drd;

							/* We allow removing disallowed directions to break up
							 * deadlocks, but adding them can break articulated
							 * vehicles. As such, only when less is disallowed,
							 * i.e. bits are removed, we skip the vehicle check. */
							if (CountBits(dis_existing) <= CountBits(dis_new)) {
								CommandCost ret = EnsureNoVehicleOnGround(tile);
								if (ret.Failed()) return ret;
							}

							/* Ignore half built tiles */
							if ((flags & DC_EXEC) && rt == ROADTYPE_ROAD && IsStraightRoad(existing)) {
								SetDisallowedRoadDirections(road_tile, dis_new);
								MarkTileDirtyByTile(tile);
							}
							return CommandCost();
						}
						return_cmd_error(STR_ERROR_ALREADY_BUILT);
					}
					/* Disallow breaking end-of-line of someone else
					 * so trams can still reverse on this tile. */
					if (rt == ROADTYPE_TRAM && HasExactlyOneBit(existing)) {
						Owner owner = GetTileOwner(road_tile);
						if (Company::IsValidID(owner)) {
							CommandCost ret = CheckOwnership(owner);
							if (ret.Failed()) return ret;
						}
					}
					break;
				}

				case ROAD_TILE_DEPOT:
					if ((DiagDirToRoadBits(GetRoadDepotDirection(road_tile)) & pieces) == pieces) return_cmd_error(STR_ERROR_ALREADY_BUILT);
					goto do_clear;

				default: NOT_REACHED();
			}
		}
	} else {
		/* No road present. */
		switch (GetTileType(tile)) {
			case MP_TUNNELBRIDGE: {
				if (GetTunnelBridgeTransportType(tile) != TRANSPORT_ROAD) goto do_clear;
				/* Only allow building the outern roadbit, so building long roads stops at existing bridges */
				if (MirrorRoadBits(DiagDirToRoadBits(GetTunnelBridgeDirection(tile))) != pieces) goto do_clear;
				if (HasTileRoadType(_m.ToTile(tile), rt)) return_cmd_error(STR_ERROR_ALREADY_BUILT);
				/* Don't allow adding roadtype to the bridge/tunnel when vehicles are already driving on it */
				CommandCost ret = TunnelBridgeIsFree(tile, GetOtherTunnelBridgeEnd(tile));
				if (ret.Failed()) return ret;
				break;
			}

			default: {
do_clear:;
				need_to_clear = true;
				break;
			}
		}
	}

	if (need_to_clear) {
		CommandCost ret = DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR);
		if (ret.Failed()) return ret;
		cost.AddCost(ret);
	}

	if (other_bits != pieces) {
		/* Check the foundation/slopes when adding road/tram bits */
		CommandCost ret = CheckRoadSlope(tileh, &pieces, existing, other_bits);
		/* Return an error if we need to build a foundation (ret != 0) but the
		 * current setting is turned off */
		if (ret.Failed() || (ret.GetCost() != 0 && !_settings_game.construction.build_on_slopes)) {
			return_cmd_error(STR_ERROR_LAND_SLOPED_IN_WRONG_DIRECTION);
		}
		cost.AddCost(ret);
	}

	if (!need_to_clear) {
		if (HasTileByType(tile, MP_ROAD)) {
			/* Don't put the pieces that already exist */
			pieces &= ComplementRoadBits(existing);

			/* Check if new road bits will have the same foundation as other existing road types */
			if (IsNormalRoadTile(tile)) {
				Slope slope = GetTileSlope(tile);
				Foundation found_new = GetRoadFoundation(slope, pieces | existing);

				if (other_bits != ROAD_NONE && GetRoadFoundation(slope, other_bits) != found_new) {
					return_cmd_error(STR_ERROR_LAND_SLOPED_IN_WRONG_DIRECTION);
				}
			}
		}

		CommandCost ret = EnsureNoVehicleOnGround(tile);
		if (ret.Failed()) return ret;
	}

	uint num_pieces = (!need_to_clear && IsTileType(tile, MP_TUNNELBRIDGE)) ?
			/* There are 2 pieces on *every* tile of the bridge or tunnel */
			2 * (GetTunnelBridgeLength(GetOtherTunnelBridgeEnd(tile), tile) + 2) :
			/* Count pieces */
			CountBits(pieces);

	cost.AddCost(num_pieces * _price[PR_BUILD_ROAD]);

	if (flags & DC_EXEC) {
		Tile *road_tile = GetRoadTileByType(tile, rt);
		if (road_tile != NULL) {
			/* Road type is already present, just add the new bits. */
			SetRoadBits(road_tile, existing | pieces);
			if (rt == ROADTYPE_ROAD) SetDisallowedRoadDirections(road_tile, IsStraightRoad(existing | pieces) ? GetDisallowedRoadDirections(road_tile) ^ toggle_drd : DRD_NONE);
			MarkTileDirtyByTile(tile);
			/* Update company infrastructure count. */
			Company *c = Company::GetIfValid(GetTileOwner(road_tile));
			if (c != NULL) {
				c->infrastructure.road[rt] += num_pieces;
				DirtyCompanyInfrastructureWindows(c->index);
			}
			return cost;
		}

		Company *c = Company::GetIfValid(company);
		if (c != NULL) DirtyCompanyInfrastructureWindows(c->index);

		switch (GetTileType(tile)) {
			case MP_TUNNELBRIDGE: {
				TileIndex other_end = GetOtherTunnelBridgeEnd(tile);

				SetRoadTypes(other_end, GetRoadTypes(other_end) | RoadTypeToRoadTypes(rt));
				SetRoadTypes(tile, GetRoadTypes(tile) | RoadTypeToRoadTypes(rt));
				SetRoadOwner(other_end, rt, company);
				SetRoadOwner(tile, rt, company);

				if (c != NULL) c->infrastructure.road[rt] += num_pieces * TUNNELBRIDGE_TRACKBIT_FACTOR;

				/* Mark tiles dirty that have been repaved */
				if (IsBridge(tile)) {
					MarkBridgeDirty(tile);
				} else {
					MarkTileDirtyByTile(other_end);
					MarkTileDirtyByTile(tile);
				}
				break;
			}

			default:
				if (HasTileByType(tile, MP_ROAD)) {
					/* Clear road side of the first tile if present. */
					SetRoadside(GetTileByType(tile, MP_ROAD), ROADSIDE_NONE);
				}
				road_tile = MakeRoadNormal(tile, pieces, rt, p2, company);
				if (rt == ROADTYPE_ROAD) {
					SetDisallowedRoadDirections(road_tile, IsStraightRoad(pieces) ? toggle_drd : DRD_NONE);
				}
				if (c != NULL) c->infrastructure.road[rt] += num_pieces;
				MakeClearGrass(tile);
				break;
		}

		MarkTileDirtyByTile(tile);
	}
	return cost;
}

/**
 * Checks whether a road or tram connection can be found when building a new road or tram.
 * @param tile Tile at which the road being built will end.
 * @param rt Roadtype of the road being built.
 * @param dir Direction that the road is following.
 * @return True if the next tile at dir direction is suitable for being connected directly by a second roadbit at the end of the road being built.
 */
static bool CanConnectToRoad(TileIndex tile, RoadType rt, DiagDirection dir)
{
	tile += TileOffsByDiagDir(dir);
	if (!IsValidTile(tile)) return false;
	RoadBits bits = GetAnyRoadBits(tile, rt, false);
	return (bits & DiagDirToRoadBits(ReverseDiagDir(dir))) != 0;
}

/**
 * Build a long piece of road.
 * @param start_tile start tile of drag (the building cost will appear over this tile)
 * @param flags operation to perform
 * @param p1 end tile of drag
 * @param p2 various bitstuffed elements
 * - p2 = (bit 0) - start tile starts in the 2nd half of tile (p2 & 1). Only used if bit 6 is set or if we are building a single tile
 * - p2 = (bit 1) - end tile starts in the 2nd half of tile (p2 & 2). Only used if bit 6 is set or if we are building a single tile
 * - p2 = (bit 2) - direction: 0 = along x-axis, 1 = along y-axis (p2 & 4)
 * - p2 = (bit 3 + 4) - road type
 * - p2 = (bit 5) - set road direction
 * - p2 = (bit 6) - defines two different behaviors for this command:
 *      - 0 = Build up to an obstacle. Do not build the first and last roadbits unless they can be connected to something, or if we are building a single tile
 *      - 1 = Fail if an obstacle is found. Always take into account bit 0 and 1. This behavior is used for scripts
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdBuildLongRoad(TileIndex start_tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	DisallowedRoadDirections drd = DRD_NORTHBOUND;

	if (p1 >= MapSize()) return CMD_ERROR;

	TileIndex end_tile = p1;
	RoadType rt = Extract<RoadType, 3, 2>(p2);
	if (!IsValidRoadType(rt) || !ValParamRoadType(rt)) return CMD_ERROR;

	Axis axis = Extract<Axis, 2, 1>(p2);
	/* Only drag in X or Y direction dictated by the direction variable */
	if (axis == AXIS_X && TileY(start_tile) != TileY(end_tile)) return CMD_ERROR; // x-axis
	if (axis == AXIS_Y && TileX(start_tile) != TileX(end_tile)) return CMD_ERROR; // y-axis

	DiagDirection dir = AxisToDiagDir(axis);

	/* Swap direction, also the half-tile drag var (bit 0 and 1) */
	if (start_tile > end_tile || (start_tile == end_tile && HasBit(p2, 0))) {
		dir = ReverseDiagDir(dir);
		p2 ^= 3;
		drd = DRD_SOUTHBOUND;
	}

	/* On the X-axis, we have to swap the initial bits, so they
	 * will be interpreted correctly in the GTTS. Furthermore
	 * when you just 'click' on one tile to build them. */
	if ((axis == AXIS_Y) == (start_tile == end_tile && HasBit(p2, 0) == HasBit(p2, 1))) drd ^= DRD_BOTH;
	/* No disallowed direction bits have to be toggled */
	if (!HasBit(p2, 5)) drd = DRD_NONE;

	CommandCost cost(EXPENSES_CONSTRUCTION);
	CommandCost last_error = CMD_ERROR;
	TileIndex tile = start_tile;
	bool had_bridge = false;
	bool had_tunnel = false;
	bool had_success = false;
	bool is_ai = HasBit(p2, 6);

	/* Start tile is the first tile clicked by the user. */
	for (;;) {
		RoadBits bits = AxisToRoadBits(axis);

		/* Determine which road parts should be built. */
		if (!is_ai && start_tile != end_tile) {
			/* Only build the first and last roadbit if they can connect to something. */
			if (tile == end_tile && !CanConnectToRoad(tile, rt, dir)) {
				bits = DiagDirToRoadBits(ReverseDiagDir(dir));
			} else if (tile == start_tile && !CanConnectToRoad(tile, rt, ReverseDiagDir(dir))) {
				bits = DiagDirToRoadBits(dir);
			}
		} else {
			/* Road parts only have to be built at the start tile or at the end tile. */
			if (tile == end_tile && !HasBit(p2, 1)) bits &= DiagDirToRoadBits(ReverseDiagDir(dir));
			if (tile == start_tile && HasBit(p2, 0)) bits &= DiagDirToRoadBits(dir);
		}

		CommandCost ret = DoCommand(tile, drd << 6 | rt << 4 | bits, 0, flags, CMD_BUILD_ROAD);
		if (ret.Failed()) {
			last_error = ret;
			if (last_error.GetErrorMessage() != STR_ERROR_ALREADY_BUILT) {
				if (is_ai) return last_error;
				break;
			}
		} else {
			had_success = true;
			/* Only pay for the upgrade on one side of the bridges and tunnels */
			if (IsTileType(tile, MP_TUNNELBRIDGE)) {
				if (IsBridge(tile)) {
					if (!had_bridge || GetTunnelBridgeDirection(tile) == dir) {
						cost.AddCost(ret);
					}
					had_bridge = true;
				} else { // IsTunnel(tile)
					if (!had_tunnel || GetTunnelBridgeDirection(tile) == dir) {
						cost.AddCost(ret);
					}
					had_tunnel = true;
				}
			} else {
				cost.AddCost(ret);
			}
		}

		if (tile == end_tile) break;

		tile += TileOffsByDiagDir(dir);
	}

	return had_success ? cost : last_error;
}

/**
 * Remove a long piece of road.
 * @param start_tile start tile of drag
 * @param flags operation to perform
 * @param p1 end tile of drag
 * @param p2 various bitstuffed elements
 * - p2 = (bit 0) - start tile starts in the 2nd half of tile (p2 & 1)
 * - p2 = (bit 1) - end tile starts in the 2nd half of tile (p2 & 2)
 * - p2 = (bit 2) - direction: 0 = along x-axis, 1 = along y-axis (p2 & 4)
 * - p2 = (bit 3 + 4) - road type
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdRemoveLongRoad(TileIndex start_tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	CommandCost cost(EXPENSES_CONSTRUCTION);

	if (p1 >= MapSize()) return CMD_ERROR;

	TileIndex end_tile = p1;
	RoadType rt = Extract<RoadType, 3, 2>(p2);
	if (!IsValidRoadType(rt)) return CMD_ERROR;

	Axis axis = Extract<Axis, 2, 1>(p2);
	/* Only drag in X or Y direction dictated by the direction variable */
	if (axis == AXIS_X && TileY(start_tile) != TileY(end_tile)) return CMD_ERROR; // x-axis
	if (axis == AXIS_Y && TileX(start_tile) != TileX(end_tile)) return CMD_ERROR; // y-axis

	/* Swap start and ending tile, also the half-tile drag var (bit 0 and 1) */
	if (start_tile > end_tile || (start_tile == end_tile && HasBit(p2, 0))) {
		TileIndex t = start_tile;
		start_tile = end_tile;
		end_tile = t;
		p2 ^= IsInsideMM(p2 & 3, 1, 3) ? 3 : 0;
	}

	Money money = GetAvailableMoneyForCommand();
	TileIndex tile = start_tile;
	CommandCost last_error = CMD_ERROR;
	bool had_success = false;
	/* Start tile is the small number. */
	for (;;) {
		RoadBits bits = AxisToRoadBits(axis);

		if (tile == end_tile && !HasBit(p2, 1)) bits &= ROAD_NW | ROAD_NE;
		if (tile == start_tile && HasBit(p2, 0)) bits &= ROAD_SE | ROAD_SW;

		/* try to remove the halves. */
		if (bits != 0) {
			CommandCost ret = RemoveRoad(tile, flags & ~DC_EXEC, bits, rt, true);
			if (ret.Succeeded()) {
				if (flags & DC_EXEC) {
					money -= ret.GetCost();
					if (money < 0) {
						_additional_cash_required = DoCommand(start_tile, end_tile, p2, flags & ~DC_EXEC, CMD_REMOVE_LONG_ROAD).GetCost();
						return cost;
					}
					RemoveRoad(tile, flags, bits, rt, true, false);
				}
				cost.AddCost(ret);
				had_success = true;
			} else {
				/* Ownership errors are more important. */
				if (last_error.GetErrorMessage() != STR_ERROR_OWNED_BY) last_error = ret;
			}
		}

		if (tile == end_tile) break;

		tile += (axis == AXIS_Y) ? TileDiffXY(0, 1) : TileDiffXY(1, 0);
	}

	return had_success ? cost : last_error;
}

/**
 * Build a road depot.
 * @param tile tile where to build the depot
 * @param flags operation to perform
 * @param p1 bit 0..1 entrance direction (DiagDirection)
 *           bit 2..3 road type
 * @param p2 unused
 * @param text unused
 * @return the cost of this operation or an error
 *
 * @todo When checking for the tile slope,
 * distinguish between "Flat land required" and "land sloped in wrong direction"
 */
CommandCost CmdBuildRoadDepot(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	DiagDirection dir = Extract<DiagDirection, 0, 2>(p1);
	RoadType rt = Extract<RoadType, 2, 2>(p1);

	if (!IsValidRoadType(rt) || !ValParamRoadType(rt)) return CMD_ERROR;

	CommandCost cost(EXPENSES_CONSTRUCTION);

	Slope tileh = GetTileSlope(tile);
	if (tileh != SLOPE_FLAT) {
		if (!_settings_game.construction.build_on_slopes || !CanBuildDepotByTileh(dir, tileh)) {
			return_cmd_error(STR_ERROR_FLAT_LAND_REQUIRED);
		}
		cost.AddCost(_price[PR_BUILD_FOUNDATION]);
	}

	cost.AddCost(DoCommand(tile, 0, 0, flags, CMD_LANDSCAPE_CLEAR));
	if (cost.Failed()) return cost;

	if (IsBridgeAbove(tile)) return_cmd_error(STR_ERROR_MUST_DEMOLISH_BRIDGE_FIRST);

	if (!Depot::CanAllocateItem()) return CMD_ERROR;

	if (flags & DC_EXEC) {
		Depot *dep = new Depot(tile);
		dep->build_date = _date;

		/* A road depot has two road bits. */
		Company::Get(_current_company)->infrastructure.road[rt] += 2;
		DirtyCompanyInfrastructureWindows(_current_company);

		MakeRoadDepot(tile, _current_company, dep->index, dir, rt);
		MarkTileDirtyByTile(tile);
		MakeDefaultName(dep);
	}
	cost.AddCost(_price[PR_BUILD_DEPOT_ROAD]);
	return cost;
}

static CommandCost RemoveRoadDepot(TileIndex tile, Tile *road_tile, DoCommandFlag flags, bool *tile_deleted)
{
	if (_current_company != OWNER_WATER) {
		CommandCost ret = CheckTileOwnership(tile, road_tile);
		if (ret.Failed()) return ret;
	}

	CommandCost ret = EnsureNoVehicleOnGround(tile);
	if (ret.Failed()) return ret;

	if (flags & DC_EXEC) {
		Company *c = Company::GetIfValid(GetTileOwner(tile));
		if (c != NULL) {
			/* A road depot has two road bits. */
			c->infrastructure.road[FIND_FIRST_BIT(GetRoadTypes(tile))] -= 2;
			DirtyCompanyInfrastructureWindows(c->index);
		}

		delete Depot::GetByTile(tile);
		_m.RemoveTile(tile, road_tile);
		MakeClearGrass(tile);
		MarkTileDirtyByTile(tile);
		*tile_deleted = true;
	}

	return CommandCost(EXPENSES_CONSTRUCTION, _price[PR_CLEAR_DEPOT_ROAD]);
}

static CommandCost ClearTile_Road(TileIndex tile, Tile *tptr, DoCommandFlag flags, bool *tile_deleted)
{
	switch (GetRoadTileType(tptr)) {
		case ROAD_TILE_NORMAL: {
			RoadType rt = (RoadType)FindFirstBit(GetRoadTypes(tptr));
			RoadBits b = GetRoadBits(tptr);

			/* Clear the road if only one piece is on the tile OR we are not using the DC_AUTO flag */
			if ((HasExactlyOneBit(b) && rt != ROADTYPE_TRAM) || !(flags & DC_AUTO)) {
				return RemoveRoadReal(tile, tptr, flags, b, rt, false, true, tile_deleted);
			}
			return_cmd_error(STR_ERROR_MUST_REMOVE_ROAD_FIRST);
		}

		default:
		case ROAD_TILE_DEPOT:
			if (flags & DC_AUTO) {
				return_cmd_error(STR_ERROR_BUILDING_MUST_BE_DEMOLISHED);
			}
			return RemoveRoadDepot(tile, tptr, flags, tile_deleted);
	}
}


struct DrawRoadTileStruct {
	uint16 image;
	byte subcoord_x;
	byte subcoord_y;
};

#include "table/road_land.h"

/**
 * Get the foundationtype of a RoadBits Slope combination
 *
 * @param tileh The Slope part
 * @param bits The RoadBits part
 * @return The resulting Foundation
 */
static Foundation GetRoadFoundation(Slope tileh, RoadBits bits)
{
	/* Flat land and land without a road doesn't require a foundation */
	if (tileh == SLOPE_FLAT || bits == ROAD_NONE) return FOUNDATION_NONE;

	/* Steep slopes behave the same as slopes with one corner raised. */
	if (IsSteepSlope(tileh)) {
		tileh = SlopeWithOneCornerRaised(GetHighestSlopeCorner(tileh));
	}

	/* Leveled RoadBits on a slope */
	if ((_invalid_tileh_slopes_road[0][tileh] & bits) == ROAD_NONE) return FOUNDATION_LEVELED;

	/* Straight roads without foundation on a slope */
	if (!IsSlopeWithOneCornerRaised(tileh) &&
			(_invalid_tileh_slopes_road[1][tileh] & bits) == ROAD_NONE)
		return FOUNDATION_NONE;

	/* Roads on steep Slopes or on Slopes with one corner raised */
	return (bits == ROAD_X ? FOUNDATION_INCLINED_X : FOUNDATION_INCLINED_Y);
}

const byte _road_sloped_sprites[14] = {
	0,  0,  2,  0,
	0,  1,  0,  0,
	3,  0,  0,  0,
	0,  0
};

/**
 * Should the road be drawn as a unpaved snow/desert road?
 * By default, roads are always drawn as unpaved if they are on desert or
 * above the snow line, but NewGRFs can override this for desert.
 *
 * @param tile The tile the road is on
 * @param roadside What sort of road this is
 * @return True if snow/desert road sprites should be used.
 */
bool DrawRoadAsSnowDesert(TileIndex tile, Roadside roadside)
{
	ClearGround ground = IsTileType(tile, MP_CLEAR) ? GetClearGround(tile) : CLEAR_GRASS;
	return ((ground == CLEAR_SNOW || ground == CLEAR_DESERT) && GetClearDensity(tile) >= 2 &&
			!(_settings_game.game_creation.landscape == LT_TROPIC && HasGrfMiscBit(GMB_DESERT_PAVED_ROADS) &&
				roadside != ROADSIDE_NONE && roadside != ROADSIDE_GRASS && roadside != ROADSIDE_GRASS_ROAD_WORKS));
}

/**
 * Draws the catenary for the given tile
 * @param ti   information about the tile (slopes, height etc)
 * @param tram the roadbits for the tram
 */
void DrawRoadCatenary(const TileInfo *ti, RoadBits tram)
{
	/* Do not draw catenary if it is invisible */
	if (IsInvisibilitySet(TO_CATENARY)) return;

	/* Don't draw the catenary under a low bridge */
	if (IsBridgeAbove(ti->tile) && !IsTransparencySet(TO_CATENARY)) {
		int height = GetBridgeHeight(GetNorthernBridgeEnd(ti->tile));

		if (height <= GetTileMaxZ(ti->tile) + 1) return;
	}

	SpriteID front;
	SpriteID back;

	if (ti->tileh != SLOPE_FLAT) {
		back  = SPR_TRAMWAY_BACK_WIRES_SLOPED  + _road_sloped_sprites[ti->tileh - 1];
		front = SPR_TRAMWAY_FRONT_WIRES_SLOPED + _road_sloped_sprites[ti->tileh - 1];
	} else {
		back  = SPR_TRAMWAY_BASE + _road_backpole_sprites_1[tram];
		front = SPR_TRAMWAY_BASE + _road_frontwire_sprites_1[tram];
	}

	AddSortableSpriteToDraw(back,  PAL_NONE, ti->x, ti->y, 16, 16, TILE_HEIGHT + BB_HEIGHT_UNDER_BRIDGE, ti->z, IsTransparencySet(TO_CATENARY));
	AddSortableSpriteToDraw(front, PAL_NONE, ti->x, ti->y, 16, 16, TILE_HEIGHT + BB_HEIGHT_UNDER_BRIDGE, ti->z, IsTransparencySet(TO_CATENARY));
}

/**
 * Draws details on/around the road
 * @param img the sprite to draw
 * @param ti  the tile to draw on
 * @param dx  the offset from the top of the BB of the tile
 * @param dy  the offset from the top of the BB of the tile
 * @param h   the height of the sprite to draw
 */
static void DrawRoadDetail(SpriteID img, const TileInfo *ti, int dx, int dy, int h)
{
	int x = ti->x | dx;
	int y = ti->y | dy;
	int z = ti->z;
	if (ti->tileh != SLOPE_FLAT) z = GetSlopePixelZ(x, y);
	AddSortableSpriteToDraw(img, PAL_NONE, x, y, 2, 2, h, z);
}

/**
 * Draw ground sprite and road pieces
 * @param ti TileInfo
 */
static void DrawRoadBits(TileInfo *ti)
{
	RoadBits bits = GetRoadBits(ti->tptr);
	Roadside roadside = GetRoadside(ti->tptr);
	RoadType rt = (RoadType)FindFirstBit(GetRoadTypes(ti->tptr));

	SpriteID image = ti->tileh != SLOPE_FLAT ? _road_sloped_sprites[ti->tileh - 1] + SPR_ROAD_SLOPE_START : _road_tile_sprites_1[bits];
	PaletteID pal = IsTileType(ti->tile, MP_CLEAR) && IsClearGround(ti->tile, CLEAR_GRASS) && GetClearDensity(ti->tile) == 0 ? PALETTE_TO_BARE_LAND : PAL_NONE;

	if (rt == ROADTYPE_ROAD || (roadside != ROADSIDE_NONE && roadside != ROADSIDE_GRASS && roadside != ROADSIDE_GRASS_ROAD_WORKS)) {
		/* The tile has a roadside. In this case always draw a road
		 * sprite first, even for tram, as there are no tram sprites
		 * with roadsides. */
		SpriteID road_offs = image;
		if (DrawRoadAsSnowDesert(ti->tile, roadside)) {
			road_offs += 19;
		} else if (roadside > ROADSIDE_GRASS && roadside != ROADSIDE_GRASS_ROAD_WORKS) {
			/* Paved road. */
			road_offs -= 19;
		}

		DrawGroundSprite(road_offs, pal);
	}

	if (rt == ROADTYPE_TRAM) {
		image -= SPR_ROAD_Y;
		image += (roadside == ROADSIDE_NONE) ? SPR_TRAMWAY_OVERLAY : SPR_TRAMWAY_TRAM;
		DrawGroundSprite(image, pal);
	}

	if (rt == ROADTYPE_ROAD) {
		DisallowedRoadDirections drd = GetDisallowedRoadDirections(ti->tptr);
		if (drd != DRD_NONE) {
			DrawGroundSpriteAt(SPR_ONEWAY_BASE + drd - 1 + ((bits == ROAD_X) ? 0 : 3), PAL_NONE, 8, 8, GetPartialPixelZ(8, 8, ti->tileh));
		}
	}

	if (HasRoadWorks(ti->tptr)) {
		/* Road works. */
		DrawGroundSprite(bits & ROAD_X ? SPR_EXCAVATION_X : SPR_EXCAVATION_Y, PAL_NONE);
		return;
	}

	if (rt == ROADTYPE_TRAM) DrawRoadCatenary(ti, bits);

	/* Return if full detail is disabled, or we are zoomed fully out. */
	if (!HasBit(_display_opt, DO_FULL_DETAIL) || _cur_dpi->zoom > ZOOM_LVL_DETAIL) return;
	/* Don't draw road details on level crossings. */
	if (IsLevelCrossingTile(ti->tile)) return;

	/* Do not draw details (street lights, trees) under low bridge */
	if (IsBridgeAbove(ti->tile) && (roadside == ROADSIDE_TREES || roadside == ROADSIDE_STREET_LIGHTS)) {
		int height = GetBridgeHeight(GetNorthernBridgeEnd(ti->tile));
		int minz = GetTileMaxZ(ti->tile) + 2;

		if (roadside == ROADSIDE_TREES) minz++;

		if (height < minz) return;
	}

	/* If there are no road bits, return, as there is nothing left to do */
	if (rt != ROADTYPE_ROAD || HasAtMostOneBit(bits)) return;

	/* Draw extra details. */
	for (const DrawRoadTileStruct *drts = _road_display_table[roadside][GetAllRoadBits(ti->tile)]; drts->image != 0; drts++) {
		DrawRoadDetail(drts->image, ti, drts->subcoord_x, drts->subcoord_y, 0x10);
	}
}

/** Tile callback function for rendering a road tile to the screen */
static void DrawTile_Road(TileInfo *ti, bool draw_halftile, Corner halftile_corner)
{
	switch (GetRoadTileType(ti->tptr)) {
		case ROAD_TILE_NORMAL:
			DrawRoadBits(ti);
			break;

		default:
		case ROAD_TILE_DEPOT: {
			PaletteID palette = COMPANY_SPRITE_COLOUR(GetTileOwner(ti->tptr));

			const DrawTileSprites *dts;
			if (HasTileRoadType(ti->tptr, ROADTYPE_TRAM)) {
				dts =  &_tram_depot[GetRoadDepotDirection(ti->tptr)];
			} else {
				dts =  &_road_depot[GetRoadDepotDirection(ti->tptr)];
			}

			DrawGroundSprite(dts->ground.sprite, PAL_NONE);
			DrawOrigTileSeq(ti, dts, TO_BUILDINGS, palette);
			break;
		}
	}
}

/**
 * Draw the road depot sprite.
 * @param x   The x offset to draw at.
 * @param y   The y offset to draw at.
 * @param dir The direction the depot must be facing.
 * @param rt  The road type of the depot to draw.
 */
void DrawRoadDepotSprite(int x, int y, DiagDirection dir, RoadType rt)
{
	PaletteID palette = COMPANY_SPRITE_COLOUR(_local_company);
	const DrawTileSprites *dts = (rt == ROADTYPE_TRAM) ? &_tram_depot[dir] : &_road_depot[dir];

	DrawSprite(dts->ground.sprite, PAL_NONE, x, y);
	DrawOrigTileSeqInGUI(x, y, dts, palette);
}

/**
 * Updates cached nearest town for all road tiles
 * @param invalidate are we just invalidating cached data?
 * @pre invalidate == true implies _generating_world == true
 */
void UpdateNearestTownForRoadTiles(bool invalidate)
{
	assert(!invalidate || _generating_world);

	for (TileIndex t = 0; t < MapSize(); t++) {
		if (HasTileByType(t, MP_ROAD) && !IsRoadDepotTile(t) && !HasTownOwnedRoad(t)) {
			TownID tid = INVALID_TOWN;
			if (!invalidate) {
				const Town *town = CalcClosestTownFromTile(t);
				if (town != NULL) tid = town->index;
			}
			FOR_ALL_ROAD_TILES(road, t) {
				SetTownIndex(road, tid);
			}
		}
	}
}

static Foundation GetFoundation_Road(TileIndex tile, Tile *tptr, Slope tileh)
{
	if (IsNormalRoad(tptr)) {
		return GetRoadFoundation(tileh, GetRoadBits(tptr));
	} else {
		return FlatteningFoundation(tileh);
	}
}

static const Roadside _town_road_types[][2] = {
	{ ROADSIDE_GRASS,         ROADSIDE_GRASS },
	{ ROADSIDE_PAVED,         ROADSIDE_PAVED },
	{ ROADSIDE_PAVED,         ROADSIDE_PAVED },
	{ ROADSIDE_TREES,         ROADSIDE_TREES },
	{ ROADSIDE_STREET_LIGHTS, ROADSIDE_PAVED }
};

static const Roadside _town_road_types_2[][2] = {
	{ ROADSIDE_GRASS,         ROADSIDE_GRASS },
	{ ROADSIDE_PAVED,         ROADSIDE_PAVED },
	{ ROADSIDE_STREET_LIGHTS, ROADSIDE_PAVED },
	{ ROADSIDE_STREET_LIGHTS, ROADSIDE_PAVED },
	{ ROADSIDE_STREET_LIGHTS, ROADSIDE_PAVED }
};


static bool TileLoop_Road(TileIndex tile, Tile *&road_tile)
{
	if (IsRoadDepot(road_tile)) return true;

	if (!HasRoadWorks(road_tile)) {
		HouseZonesBits grp = HZB_TOWN_EDGE;

		const Town *t = ClosestTownFromTile(tile, UINT_MAX);
		if (t != NULL) {
			grp = GetTownRadiusGroup(t, tile);

			/* Show an animation to indicate road work. Only the last
			 * associated road tile at a tile index can have road works. */
			if (t->road_build_months != 0 &&
					(DistanceManhattan(t->xy, tile) < 8 || grp != HZB_TOWN_EDGE) &&
					IsNormalRoad(road_tile) && !HasAtMostOneBit(GetAllRoadBits(tile)) &&
					!HasTileByType(tile, MP_RAILWAY) && GetNextTileByType(road_tile, MP_ROAD) == NULL) {
				if (GetFoundationSlope(tile) == SLOPE_FLAT && EnsureNoVehicleOnGround(tile).Succeeded() && Chance16(1, 40)) {
					StartRoadWorks(road_tile);

					if (_settings_client.sound.ambient) SndPlayTileFx(SND_21_JACKHAMMER, tile);
					CreateEffectVehicleAbove(
						TileX(tile) * TILE_SIZE + 7,
						TileY(tile) * TILE_SIZE + 7,
						0,
						EV_BULLDOZER);
					MarkTileDirtyByTile(tile);
					return true;
				}
			}
		}

		{
			/* Adjust road ground type depending on 'grp' (grp is the distance to the center) */
			const Roadside *new_rs = (_settings_game.game_creation.landscape == LT_TOYLAND) ? _town_road_types_2[grp] : _town_road_types[grp];
			Roadside desired = new_rs[0];
			Roadside pre     = new_rs[1];
			Roadside cur_rs  = GetRoadside(road_tile);

			/* Only change the road side for the first associated road tile. */
			if (road_tile != GetTileByType(tile, MP_ROAD)) return true;

			/* Road stops are always on paved ground. */
			if (HasTileByType(tile, MP_STATION)) {
				desired = ROADSIDE_PAVED;
				pre = cur_rs;
			}

			/* We have our desired type, do nothing */
			if (cur_rs == desired) return true;

			if (HasTileByType(tile, MP_RAILWAY)) {
				/* No trees or lights for level crossings. */
				if (desired > ROADSIDE_PAVED) desired = ROADSIDE_PAVED;
				if (pre > ROADSIDE_PAVED) pre = ROADSIDE_PAVED;
			}

			/* We have the pre-type of the desired type, switch to the desired type */
			if (cur_rs == pre) {
				cur_rs = desired;
			/* We have barren land, install the pre-type */
			} else if (cur_rs == ROADSIDE_NONE) {
				cur_rs = pre;
			/* We're totally off limits, remove any installation and make barren land */
			} else {
				cur_rs = ROADSIDE_NONE;
			}
			SetRoadside(road_tile, cur_rs);
			MarkTileDirtyByTile(tile);
		}
	} else if (IncreaseRoadWorksCounter(road_tile)) {
		TerminateRoadWorks(road_tile);

		if (_settings_game.economy.mod_road_rebuild) {
			/* Generate a nicer town surface */
			const RoadBits old_rb = GetRoadBits(tile, ROADTYPE_ROAD);
			const RoadBits new_rb = CleanUpRoadBits(tile, old_rb);

			if (old_rb != new_rb) {
				RoadType rt = (RoadType)FindFirstBit(GetRoadTypes(road_tile));
				bool tile_removed = false;
				RemoveRoadReal(tile, GetRoadTileByType(tile, ROADTYPE_ROAD), DC_EXEC | DC_AUTO | DC_NO_WATER, (old_rb ^ new_rb), ROADTYPE_ROAD, true, true, &tile_removed);
				/* We removed our current tile? Don't jump to next. */
				if (tile_removed && rt == ROADTYPE_ROAD) return false;
				/* Otherwise recalc the tile pointer. */
				road_tile = GetRoadTileByType(tile, rt);
			}
		}

		MarkTileDirtyByTile(tile);
	}
	return true;
}

static bool ClickTile_Road(TileIndex tile, Tile *tptr)
{
	if (!IsRoadDepot(tptr)) return false;

	ShowDepotWindow(tile, VEH_ROAD);
	return true;
}

/* Converts RoadBits to TrackBits */
static const TrackBits _road_trackbits[16] = {
	TRACK_BIT_NONE,                                  // ROAD_NONE
	TRACK_BIT_NONE,                                  // ROAD_NW
	TRACK_BIT_NONE,                                  // ROAD_SW
	TRACK_BIT_LEFT,                                  // ROAD_W
	TRACK_BIT_NONE,                                  // ROAD_SE
	TRACK_BIT_Y,                                     // ROAD_Y
	TRACK_BIT_LOWER,                                 // ROAD_S
	TRACK_BIT_LEFT | TRACK_BIT_LOWER | TRACK_BIT_Y,  // ROAD_Y | ROAD_SW
	TRACK_BIT_NONE,                                  // ROAD_NE
	TRACK_BIT_UPPER,                                 // ROAD_N
	TRACK_BIT_X,                                     // ROAD_X
	TRACK_BIT_LEFT | TRACK_BIT_UPPER | TRACK_BIT_X,  // ROAD_X | ROAD_NW
	TRACK_BIT_RIGHT,                                 // ROAD_E
	TRACK_BIT_RIGHT | TRACK_BIT_UPPER | TRACK_BIT_Y, // ROAD_Y | ROAD_NE
	TRACK_BIT_RIGHT | TRACK_BIT_LOWER | TRACK_BIT_X, // ROAD_X | ROAD_SE
	TRACK_BIT_ALL,                                   // ROAD_ALL
};

static TrackStatus GetTileTrackStatus_Road(TileIndex tile, Tile *road_tile, TransportType mode, uint sub_mode, DiagDirection side)
{
	TrackdirBits trackdirbits = TRACKDIR_BIT_NONE;
	switch (mode) {
		case TRANSPORT_ROAD:
			if ((GetRoadTypes(road_tile) & sub_mode) == 0) break;
			switch (GetRoadTileType(road_tile)) {
				case ROAD_TILE_NORMAL: {
					const uint drd_to_multiplier[DRD_END] = { 0x101, 0x100, 0x1, 0x0 };
					RoadType rt = (RoadType)FindFirstBit(sub_mode);
					RoadBits bits = GetRoadBits(road_tile);

					/* no roadbit at this side of tile, return 0 */
					if (side != INVALID_DIAGDIR && (DiagDirToRoadBits(side) & bits) == 0) break;

					uint multiplier = drd_to_multiplier[rt == ROADTYPE_TRAM ? DRD_NONE : GetDisallowedRoadDirections(road_tile)];
					if (!HasRoadWorks(road_tile)) trackdirbits = (TrackdirBits)(_road_trackbits[bits] * multiplier);
					break;
				}

				default:
				case ROAD_TILE_DEPOT: {
					DiagDirection dir = GetRoadDepotDirection(road_tile);

					if (side != INVALID_DIAGDIR && side != dir) break;

					trackdirbits = TrackBitsToTrackdirBits(DiagDirToDiagTrackBits(dir));
					break;
				}
			}
			break;

		default: break;
	}
	return CombineTrackStatus(trackdirbits, TRACKDIR_BIT_NONE);
}

static const StringID _road_tile_strings[] = {
	STR_LAI_ROAD_DESCRIPTION_ROAD,
	STR_LAI_ROAD_DESCRIPTION_ROAD,
	STR_LAI_ROAD_DESCRIPTION_ROAD,
	STR_LAI_ROAD_DESCRIPTION_ROAD_WITH_STREETLIGHTS,
	STR_LAI_ROAD_DESCRIPTION_ROAD,
	STR_LAI_ROAD_DESCRIPTION_TREE_LINED_ROAD,
	STR_LAI_ROAD_DESCRIPTION_ROAD,
	STR_LAI_ROAD_DESCRIPTION_ROAD,
};

static void GetTileDesc_Road(TileIndex tile, Tile *road_tile, TileDesc *td)
{
	RoadTypes rts = GetRoadTypes(road_tile);

	switch (GetRoadTileType(road_tile)) {
		case ROAD_TILE_DEPOT:
			td->str = STR_LAI_ROAD_DESCRIPTION_ROAD_VEHICLE_DEPOT;
			td->build_date = Depot::GetByTile(tile)->build_date;
			break;

		default: {
			if (IsLevelCrossingTile(tile)) {
				td->str = STR_LAI_ROAD_DESCRIPTION_ROAD_RAIL_LEVEL_CROSSING;
			} else {
				td->str = (HasBit(rts, ROADTYPE_ROAD) ? _road_tile_strings[GetRoadside(road_tile)] : STR_LAI_ROAD_DESCRIPTION_TRAMWAY);
			}
			break;
		}
	}

	/* Determine owner string. */
	td->owner[0] = GetTileOwner(road_tile);
	if (td->owner[0] != OWNER_NONE) td->owner_type[0] = HasBit(rts, ROADTYPE_TRAM) ? STR_LAND_AREA_INFORMATION_TRAM_OWNER : STR_LAND_AREA_INFORMATION_ROAD_OWNER;
}

/**
 * Given the direction the road depot is pointing, this is the direction the
 * vehicle should be travelling in in order to enter the depot.
 */
static const byte _roadveh_enter_depot_dir[4] = {
	TRACKDIR_X_SW, TRACKDIR_Y_NW, TRACKDIR_X_NE, TRACKDIR_Y_SE
};

static VehicleEnterTileStatus VehicleEnter_Road(Vehicle *v, TileIndex tile, Tile *road_tile, int x, int y)
{
	switch (GetRoadTileType(road_tile)) {
		case ROAD_TILE_DEPOT: {
			if (v->type != VEH_ROAD) break;

			RoadVehicle *rv = RoadVehicle::From(v);
			if (rv->frame == RVC_DEPOT_STOP_FRAME &&
					_roadveh_enter_depot_dir[GetRoadDepotDirection(road_tile)] == rv->state) {
				rv->state = RVSB_IN_DEPOT;
				rv->vehstatus |= VS_HIDDEN;
				rv->direction = ReverseDir(rv->direction);
				if (rv->Next() == NULL) VehicleEnterDepot(rv->First());
				rv->tile = tile;

				InvalidateWindowData(WC_VEHICLE_DEPOT, rv->tile);
				return VETSB_ENTERED_WORMHOLE;
			}
			break;
		}

		default: break;
	}
	return VETSB_CONTINUE;
}


static bool ChangeTileOwner_Road(TileIndex tile, Tile *road_tile, Owner old_owner, Owner new_owner)
{
	if (GetTileOwner(road_tile) == old_owner) {
		RoadType rt = (RoadType)FIND_FIRST_BIT(GetRoadTypes(road_tile));
		uint pieces = IsRoadDepot(road_tile) ? 2 : CountBits(GetRoadBits(road_tile));
		Company::Get(old_owner)->infrastructure.road[rt] -= pieces;

		if (new_owner == INVALID_OWNER) {
			if (IsRoadDepot(road_tile)) {
				DoCommand(tile, 0, 0, DC_EXEC | DC_BANKRUPT, CMD_LANDSCAPE_CLEAR);
			} else {
				SetTileOwner(road_tile, OWNER_NONE);
			}
		} else {
			SetTileOwner(road_tile, new_owner);
			Company::Get(new_owner)->infrastructure.road[rt] += pieces;
		}
	}

	return true;
}

static CommandCost TerraformTile_Road(TileIndex tile, Tile *road_tile, DoCommandFlag flags, int z_new, Slope tileh_new)
{
	if (_settings_game.construction.build_on_slopes && AutoslopeEnabled()) {
		switch (GetRoadTileType(road_tile)) {
			case ROAD_TILE_DEPOT:
				if (AutoslopeCheckForEntranceEdge(tile, z_new, tileh_new, GetRoadDepotDirection(road_tile))) return CommandCost(EXPENSES_CONSTRUCTION, _price[PR_BUILD_FOUNDATION]);
				break;

			case ROAD_TILE_NORMAL: {
				RoadBits bits = GetAllRoadBits(tile);
				RoadBits bits_copy = bits;
				/* Check if the slope-road_bits combination is valid at all, i.e. it is safe to call GetRoadFoundation(). */
				if (CheckRoadSlope(tileh_new, &bits_copy, ROAD_NONE, ROAD_NONE).Succeeded()) {
					/* CheckRoadSlope() sometimes changes the road_bits, if it does not agree with them. */
					if (bits == bits_copy) {
						int z_old;
						Slope tileh_old = GetTileSlope(tile, &z_old);

						/* Get the slope on top of the foundation */
						z_old += ApplyFoundationToSlope(GetRoadFoundation(tileh_old, bits), &tileh_old);
						z_new += ApplyFoundationToSlope(GetRoadFoundation(tileh_new, bits), &tileh_new);

						/* The surface slope must not be changed */
						if ((z_old == z_new) && (tileh_old == tileh_new)) return CommandCost(EXPENSES_CONSTRUCTION, _price[PR_BUILD_FOUNDATION]);
					}
				}
				break;
			}

			default: NOT_REACHED();
		}
	}

	return CommandCost(INVALID_STRING_ID); // Dummy error
}

/** Tile callback functions for road tiles */
extern const TileTypeProcs _tile_type_road_procs = {
	DrawTile_Road,           // draw_tile_proc
	ClearTile_Road,          // clear_tile_proc
	NULL,                    // add_accepted_cargo_proc
	GetTileDesc_Road,        // get_tile_desc_proc
	GetTileTrackStatus_Road, // get_tile_track_status_proc
	ClickTile_Road,          // click_tile_proc
	NULL,                    // animate_tile_proc
	TileLoop_Road,           // tile_loop_proc
	ChangeTileOwner_Road,    // change_tile_owner_proc
	NULL,                    // add_produced_cargo_proc
	VehicleEnter_Road,       // vehicle_enter_tile_proc
	GetFoundation_Road,      // get_foundation_proc
	TerraformTile_Road,      // terraform_tile_proc
};
