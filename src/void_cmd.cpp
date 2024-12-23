/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file void_cmd.cpp Handling of void tiles. */

#include "stdafx.h"
#include "landscape.h"
#include "command_func.h"
#include "viewport_func.h"
#include "slope_func.h"
#include "water.h"

#include "table/strings.h"
#include "table/sprites.h"

#include "safeguards.h"

static void DrawTile_Void(TileInfo *ti)
{
	DrawGroundSprite(SPR_FLAT_BARE_LAND + SlopeToSpriteOffset(ti->tileh), PALETTE_ALL_BLACK);
}


static Foundation GetFoundation_Void(TileIndex, Slope)
{
	return FOUNDATION_NONE;
}

static std::tuple<CommandCost, bool> ClearTile_Void(TileIndex, Tile &, DoCommandFlag)
{
	return {CommandCost(STR_ERROR_OFF_EDGE_OF_MAP), false};
}


static void GetTileDesc_Void(TileIndex, Tile, TileDesc *td)
{
	td->str = STR_EMPTY;
	td->owner[0] = OWNER_NONE;
}

static bool TileLoop_Void(TileIndex index, Tile &tile)
{
	/* Floods adjacent edge tile to prevent maps without water. */
	return TileLoop_Water(index, tile);
}

static bool ChangeTileOwner_Void(TileIndex, Tile &, Owner, Owner)
{
	/* not used */
	return false;
}

static TrackStatus GetTileTrackStatus_Void(TileIndex, TransportType, uint, DiagDirection)
{
	return 0;
}

static CommandCost TerraformTile_Void(TileIndex, Tile, DoCommandFlag, int, Slope)
{
	return CommandCost(STR_ERROR_OFF_EDGE_OF_MAP);
}

extern const TileTypeProcs _tile_type_void_procs = {
	DrawTile_Void,            // draw_tile_proc
	ClearTile_Void,           // clear_tile_proc
	nullptr,                     // add_accepted_cargo_proc
	GetTileDesc_Void,         // get_tile_desc_proc
	GetTileTrackStatus_Void,  // get_tile_track_status_proc
	nullptr,                     // click_tile_proc
	nullptr,                     // animate_tile_proc
	TileLoop_Void,            // tile_loop_proc
	ChangeTileOwner_Void,     // change_tile_owner_proc
	nullptr,                     // add_produced_cargo_proc
	nullptr,                     // vehicle_enter_tile_proc
	GetFoundation_Void,       // get_foundation_proc
	TerraformTile_Void,       // terraform_tile_proc
};
