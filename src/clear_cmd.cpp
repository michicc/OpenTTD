/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file clear_cmd.cpp Commands related to clear tiles. */

#include "stdafx.h"
#include "clear_map.h"
#include "command_func.h"
#include "landscape.h"
#include "genworld.h"
#include "viewport_func.h"
#include "core/random_func.hpp"
#include "newgrf_generic.h"
#include "landscape_cmd.h"

#include "table/strings.h"
#include "table/sprites.h"
#include "table/clear_land.h"

#include "safeguards.h"

static std::tuple<CommandCost, bool> ClearTile_Clear(TileIndex index, Tile &tile, DoCommandFlag flags)
{
	static const Price clear_price_table[] = {
		PR_CLEAR_GRASS,
		PR_CLEAR_ROUGH,
		PR_CLEAR_ROCKS,
		PR_CLEAR_FIELDS,
		PR_CLEAR_ROUGH,
		PR_CLEAR_ROUGH,
	};
	CommandCost price(EXPENSES_CONSTRUCTION);

	if (!Tile::HasType(index, MP_TREES) && (!IsClearGround(tile, CLEAR_GRASS) || GetClearDensity(tile) != 0)) {
		price.AddCost(_price[clear_price_table[GetClearGround(tile)]]);
	}

	if (flags & DC_EXEC) {
		MakeClearGrass(tile);
		MarkTileDirtyByTile(index);
	}

	return {price, false};
}

static inline std::tuple<uint, const SubSprite *> GetHigherHalftileSubsprite(const TileInfo *ti, bool draw_halftile, Corner halftile_corner)
{
	if (draw_halftile) {
		/* Use the sloped sprites with three corners raised. They probably best fit the lightning for the higher half-tile. */
		Slope fake_slope = SlopeWithThreeCornersRaised(OppositeCorner(halftile_corner));
		return {SlopeToSpriteOffset(fake_slope), GetHalftileSubSprite(halftile_corner)};
	} else {
		return {SlopeToSpriteOffset(ti->tileh), nullptr};
	}
}

void DrawClearLandTile(const TileInfo *ti, uint8_t set, bool draw_halftile, Corner halftile_corner)
{
	auto [offset, subsprite] = GetHigherHalftileSubsprite(ti, draw_halftile, halftile_corner);
	DrawGroundSprite(SPR_FLAT_BARE_LAND + offset + set * 19, PAL_NONE, subsprite);
}

void DrawHillyLandTile(const TileInfo *ti, bool draw_halftile, Corner halftile_corner)
{
	auto [offset, subsprite] = GetHigherHalftileSubsprite(ti, draw_halftile, halftile_corner);
	if (ti->tileh != SLOPE_FLAT || draw_halftile) {
		DrawGroundSprite(SPR_FLAT_ROUGH_LAND + offset, PAL_NONE, subsprite);
	} else {
		DrawGroundSprite(_landscape_clear_sprites_rough[GB(TileHash(ti->x, ti->y), 0, 3)], PAL_NONE);
	}
}

static void DrawRockLandTile(const TileInfo *ti, bool draw_halftile, Corner halftile_corner)
{
	SpriteID rocks = HasGrfMiscBit(GMB_SECOND_ROCKY_TILE_SET) && (TileHash(ti->x, ti->y) & 1) ? SPR_FLAT_ROCKY_LAND_2 : SPR_FLAT_ROCKY_LAND_1;
	auto [offset, subsprite] = GetHigherHalftileSubsprite(ti, draw_halftile, halftile_corner);
	DrawGroundSprite(rocks + offset, PAL_NONE, subsprite);
}

static void DrawSnowDesertTile(const TileInfo *ti, bool draw_halftile, Corner halftile_corner)
{
	/* If the tile has snow, increase the density for the higher halftile by one to match the surrounding tiles. */
	uint density = GetClearDensity(ti->tile);
	if (draw_halftile && IsSnowTile(ti->tile) && density < 3) density++;

	auto [offset, subsprite] = GetHigherHalftileSubsprite(ti, draw_halftile, halftile_corner);
	DrawGroundSprite(_clear_land_sprites_snow_desert[density] + offset, PAL_NONE, subsprite);
}

static void DrawFieldTile(const TileInfo *ti, bool draw_halftile, Corner halftile_corner)
{
	auto [offset, subsprite] = GetHigherHalftileSubsprite(ti, draw_halftile, halftile_corner);
	DrawGroundSprite(_clear_land_sprites_farmland[GetFieldType(ti->tile)] + offset, PAL_NONE, subsprite);
}

static void DrawClearLandFence(const TileInfo *ti, bool draw_halftile, Corner halftile_corner)
{
	/* combine fences into one sprite object */
	StartSpriteCombine();

	int maxz = GetSlopeMaxPixelZ(ti->tileh);

	uint fence_nw = GetFence(ti->tile, DIAGDIR_NW);
	uint fence_ne = GetFence(ti->tile, DIAGDIR_NE);
	uint fence_sw = GetFence(ti->tile, DIAGDIR_SW);
	uint fence_se = GetFence(ti->tile, DIAGDIR_SE);

	if (IsValidCorner(halftile_corner)) {
		/* Tile has a half-tile foundation. Draw a fence that touches the half-tile corner only when
		 * the half-tile is drawn and fences that don't touch only when the normal tile is drawn. */
		if (!(draw_halftile ^ (halftile_corner != CORNER_N && halftile_corner != CORNER_W))) fence_nw = 0;
		if (!(draw_halftile ^ (halftile_corner != CORNER_N && halftile_corner != CORNER_E))) fence_ne = 0;
		if (!(draw_halftile ^ (halftile_corner != CORNER_S && halftile_corner != CORNER_W))) fence_sw = 0;
		if (!(draw_halftile ^ (halftile_corner != CORNER_S && halftile_corner != CORNER_E))) fence_se = 0;
	}
	Slope s = IsHalftileSlope(ti->tileh) ? SLOPE_ELEVATED : ti->tileh;

	if (fence_nw != 0) {
		int z = GetSlopePixelZInCorner(ti->tileh, CORNER_W);
		SpriteID sprite = _clear_land_fence_sprites[fence_nw - 1] + _fence_mod_by_tileh_nw[s];
		AddSortableSpriteToDraw(sprite, PAL_NONE, ti->x, ti->y - 16, 16, 32, maxz - z + 4, ti->z + z, false, 0, 16, -z);
	}

	if (fence_ne != 0) {
		int z = GetSlopePixelZInCorner(ti->tileh, CORNER_E);
		SpriteID sprite = _clear_land_fence_sprites[fence_ne - 1] + _fence_mod_by_tileh_ne[s];
		AddSortableSpriteToDraw(sprite, PAL_NONE, ti->x - 16, ti->y, 32, 16, maxz - z + 4, ti->z + z, false, 16, 0, -z);
	}

	if (fence_sw != 0 || fence_se != 0) {
		int z = GetSlopePixelZInCorner(s, CORNER_S);

		if (fence_sw != 0) {
			SpriteID sprite = _clear_land_fence_sprites[fence_sw - 1] + _fence_mod_by_tileh_sw[s];
			AddSortableSpriteToDraw(sprite, PAL_NONE, ti->x, ti->y, 16, 16, maxz - z + 4, ti->z + z, false, 0, 0, -z);
		}

		if (fence_se != 0) {
			SpriteID sprite = _clear_land_fence_sprites[fence_se - 1] + _fence_mod_by_tileh_se[s];
			AddSortableSpriteToDraw(sprite, PAL_NONE, ti->x, ti->y, 16, 16, maxz - z + 4, ti->z + z, false, 0, 0, -z);
		}
	}
	EndSpriteCombine();
}

static void DrawTile_Clear(TileInfo *ti, bool draw_halftile, Corner halftile_corner)
{
	switch (GetClearGround(ti->tile)) {
		case CLEAR_GRASS:
			DrawClearLandTile(ti, GetClearDensity(ti->tile), draw_halftile, halftile_corner);
			break;

		case CLEAR_ROUGH:
			DrawHillyLandTile(ti, draw_halftile, halftile_corner);
			break;

		case CLEAR_ROCKS:
			DrawRockLandTile(ti, draw_halftile, halftile_corner);
			break;

		case CLEAR_FIELDS:
			DrawFieldTile(ti, draw_halftile, halftile_corner);
			DrawClearLandFence(ti, draw_halftile, halftile_corner);
			break;

		case CLEAR_SNOW:
		case CLEAR_DESERT:
			DrawSnowDesertTile(ti, draw_halftile, halftile_corner);
			break;
	}
}

static Foundation GetFoundation_Clear(TileIndex, Tile, Slope)
{
	return FOUNDATION_NONE;
}

static void UpdateFences(TileIndex tile)
{
	assert(IsTileType(tile, MP_CLEAR) && IsClearGround(tile, CLEAR_FIELDS));
	bool dirty = false;

	for (DiagDirection dir = DIAGDIR_BEGIN; dir < DIAGDIR_END; dir++) {
		if (GetFence(tile, dir) != 0) continue;
		TileIndex neighbour = tile + TileOffsByDiagDir(dir);
		if (IsTileType(neighbour, MP_CLEAR) && IsClearGround(neighbour, CLEAR_FIELDS)) continue;
		SetFence(tile, dir, 3);
		dirty = true;
	}

	if (dirty) MarkTileDirtyByTile(tile);
}


/** Convert to or from snowy tiles. */
static void TileLoopClearAlps(TileIndex tile)
{
	int k = std::get<1>(GetFoundationSlope(tile)) - GetSnowLine() + 1;

	if (!IsSnowTile(tile)) {
		/* Below the snow line, do nothing if no snow. */
		/* At or above the snow line, make snow tile if needed. */
		if (k >= 0) {
			MakeSnow(tile);
			MarkTileDirtyByTile(tile);
		}
		return;
	}

	/* Update snow density. */
	uint current_density = GetClearDensity(tile);
	uint req_density = (k < 0) ? 0u : std::min<uint>(k, 3u);

	if (current_density == req_density) {
		/* Density at the required level. */
		if (k >= 0) return;
		ClearSnow(tile);
	} else {
		AddClearDensity(tile, current_density < req_density ? 1 : -1);
	}

	MarkTileDirtyByTile(tile);
}

/**
 * Tests if at least one surrounding tile is non-desert
 * @param tile tile to check
 * @return does this tile have at least one non-desert tile around?
 */
static inline bool NeighbourIsNormal(TileIndex tile)
{
	for (DiagDirection dir = DIAGDIR_BEGIN; dir < DIAGDIR_END; dir++) {
		TileIndex t = tile + TileOffsByDiagDir(dir);
		if (!IsValidTile(t)) continue;
		if (GetTropicZone(t) != TROPICZONE_DESERT) return true;
		if (HasTileWaterClass(t) && GetWaterClass(t) == WATER_CLASS_SEA) return true;
	}
	return false;
}

static void TileLoopClearDesert(TileIndex tile)
{
	/* Current desert level - 0 if it is not desert */
	uint current = 0;
	if (IsClearGround(tile, CLEAR_DESERT)) current = GetClearDensity(tile);

	/* Expected desert level - 0 if it shouldn't be desert */
	uint expected = 0;
	if (GetTropicZone(tile) == TROPICZONE_DESERT) {
		expected = NeighbourIsNormal(tile) ? 1 : 3;
	}

	if (current == expected) return;

	if (expected == 0) {
		SetClearGroundDensity(tile, CLEAR_GRASS, 3);
	} else {
		/* Transition from clear to desert is not smooth (after clearing desert tile) */
		SetClearGroundDensity(tile, CLEAR_DESERT, expected);
	}

	MarkTileDirtyByTile(tile);
}

static bool TileLoop_Clear(TileIndex index, Tile &tile)
{
	AmbientSoundEffect(index);

	switch (_settings_game.game_creation.landscape) {
		case LT_TROPIC: TileLoopClearDesert(index); break;
		case LT_ARCTIC: TileLoopClearAlps(index);   break;
	}

	switch (GetClearGround(tile)) {
		case CLEAR_GRASS:
			if (GetClearDensity(tile) == 3) return false;

			if (_game_mode != GM_EDITOR) {
				if (GetClearCounter(tile) < 7) {
					AddClearCounter(tile, 1);
					return true;
				} else {
					SetClearCounter(tile, 0);
					AddClearDensity(tile, 1);
				}
			} else {
				SetClearGroundDensity(tile, GB(Random(), 0, 8) > 21 ? CLEAR_GRASS : CLEAR_ROUGH, 3);
			}
			break;

		case CLEAR_FIELDS:
			UpdateFences(index);

			if (_game_mode == GM_EDITOR) return false;

			if (GetClearCounter(tile) < 7) {
				AddClearCounter(tile, 1);
				return false;
			} else {
				SetClearCounter(tile, 0);
			}

			if (GetIndustryIndexOfField(tile) == INVALID_INDUSTRY && GetFieldType(tile) >= 7) {
				/* This farmfield is no longer farmfield, so make it grass again */
				MakeClear(tile, CLEAR_GRASS, 2);
			} else {
				uint field_type = GetFieldType(tile);
				field_type = (field_type < 8) ? field_type + 1 : 0;
				SetFieldType(tile, field_type);
			}
			break;

		default:
			return false;
	}

	MarkTileDirtyByTile(index);
	return false;
}

void GenerateClearTile()
{
	uint i, gi;
	TileIndex tile;

	/* add rough tiles */
	i = Map::ScaleBySize(GB(Random(), 0, 10) + 0x400);
	gi = Map::ScaleBySize(GB(Random(), 0, 7) + 0x80);

	SetGeneratingWorldProgress(GWP_ROUGH_ROCKY, gi + i);
	do {
		IncreaseGeneratingWorldProgress(GWP_ROUGH_ROCKY);
		tile = RandomTile();
		if (IsTileType(tile, MP_CLEAR) && !IsClearGround(tile, CLEAR_DESERT)) SetClearGroundDensity(tile, CLEAR_ROUGH, 3);
	} while (--i);

	/* add rocky tiles */
	i = gi;
	do {
		uint32_t r = Random();
		tile = RandomTileSeed(r);

		IncreaseGeneratingWorldProgress(GWP_ROUGH_ROCKY);
		if (IsTileType(tile, MP_CLEAR) && !IsClearGround(tile, CLEAR_DESERT)) {
			uint j = GB(r, 16, 4) + 5;
			for (;;) {
				TileIndex tile_new;

				SetClearGroundDensity(tile, CLEAR_ROCKS, 3);
				MarkTileDirtyByTile(tile);
				do {
					if (--j == 0) goto get_out;
					tile_new = tile + TileOffsByDiagDir((DiagDirection)GB(Random(), 0, 2));
				} while (!IsTileType(tile_new, MP_CLEAR) || IsClearGround(tile_new, CLEAR_DESERT));
				tile = tile_new;
			}
get_out:;
		}
	} while (--i);
}

static TrackStatus GetTileTrackStatus_Clear(TileIndex, Tile, TransportType, uint, DiagDirection)
{
	return 0;
}

static const StringID _clear_land_str[] = {
	STR_LAI_CLEAR_DESCRIPTION_GRASS,
	STR_LAI_CLEAR_DESCRIPTION_ROUGH_LAND,
	STR_LAI_CLEAR_DESCRIPTION_ROCKS,
	STR_LAI_CLEAR_DESCRIPTION_FIELDS,
	STR_LAI_CLEAR_DESCRIPTION_SNOW_COVERED_LAND,
	STR_LAI_CLEAR_DESCRIPTION_DESERT
};

static void GetTileDesc_Clear(TileIndex, Tile tile, TileDesc *td)
{
	if (IsClearGround(tile, CLEAR_GRASS) && GetClearDensity(tile) == 0) {
		td->str = STR_LAI_CLEAR_DESCRIPTION_BARE_LAND;
	} else {
		td->str = _clear_land_str[GetClearGround(tile)];
	}
	td->owner[0] = GetTileOwner(tile);
}

static bool ChangeTileOwner_Clear(TileIndex, Tile &, Owner, Owner)
{
	return false;
}

static CommandCost TerraformTile_Clear(TileIndex, Tile, DoCommandFlag, int, Slope)
{
	return CommandCost(INVALID_STRING_ID); // Dummy error
}

extern const TileTypeProcs _tile_type_clear_procs = {
	DrawTile_Clear,           ///< draw_tile_proc
	ClearTile_Clear,          ///< clear_tile_proc
	nullptr,                     ///< add_accepted_cargo_proc
	GetTileDesc_Clear,        ///< get_tile_desc_proc
	GetTileTrackStatus_Clear, ///< get_tile_track_status_proc
	nullptr,                     ///< click_tile_proc
	nullptr,                     ///< animate_tile_proc
	TileLoop_Clear,           ///< tile_loop_proc
	ChangeTileOwner_Clear,    ///< change_tile_owner_proc
	nullptr,                     ///< add_produced_cargo_proc
	nullptr,                     ///< vehicle_enter_tile_proc
	GetFoundation_Clear,      ///< get_foundation_proc
	TerraformTile_Clear,      ///< terraform_tile_proc
};
