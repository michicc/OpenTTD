/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file track_land.h Sprites to use and how to display them for train depot tiles. */

#define TILE_SEQ_LINE(img, dx, dy, sx, sy) { dx, dy, 0, sx, sy, 23, {img, PAL_NONE} },
#define TILE_SEQ_LINE2(img, pal, dx, dy, sx, sy) { dx, dy, 0, sx, sy, 20, {img, pal} }, // For level crossing
#define TILE_SEQ_END() { (int8_t)0x80, 0, 0, 0, 0, 0, {0, 0} }


static const DrawTileSeqStruct _depot_gfx_NE[] = {
	TILE_SEQ_LINE(SPR_RAIL_DEPOT_NE | (1 << PALETTE_MODIFIER_COLOUR), 2, 13, 13, 1)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _depot_gfx_SE[] = {
	TILE_SEQ_LINE(SPR_RAIL_DEPOT_SE_1 | (1 << PALETTE_MODIFIER_COLOUR),  2, 2, 1, 13)
	TILE_SEQ_LINE(SPR_RAIL_DEPOT_SE_2 | (1 << PALETTE_MODIFIER_COLOUR), 13, 2, 1, 13)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _depot_gfx_SW[] = {
	TILE_SEQ_LINE(SPR_RAIL_DEPOT_SW_1 | (1 << PALETTE_MODIFIER_COLOUR), 2,  2, 13, 1)
	TILE_SEQ_LINE(SPR_RAIL_DEPOT_SW_2 | (1 << PALETTE_MODIFIER_COLOUR), 2, 13, 13, 1)
	TILE_SEQ_END()
};

static const DrawTileSeqStruct _depot_gfx_NW[] = {
	TILE_SEQ_LINE(SPR_RAIL_DEPOT_NW | (1 << PALETTE_MODIFIER_COLOUR), 13, 2, 1, 13)
	TILE_SEQ_END()
};

static const DrawTileSprites _depot_gfx_table[] = {
	{ {0,                   PAL_NONE}, _depot_gfx_NE },
	{ {SPR_RAIL_TRACK_Y,    PAL_NONE}, _depot_gfx_SE },
	{ {SPR_RAIL_TRACK_X,    PAL_NONE}, _depot_gfx_SW },
	{ {0,                   PAL_NONE}, _depot_gfx_NW }
};

static const DrawTileSprites _depot_invisible_gfx_table[] = {
	{ {SPR_RAIL_TRACK_X, PAL_NONE}, _depot_gfx_NE },
	{ {SPR_RAIL_TRACK_Y, PAL_NONE}, _depot_gfx_SE },
	{ {SPR_RAIL_TRACK_X, PAL_NONE}, _depot_gfx_SW },
	{ {SPR_RAIL_TRACK_Y, PAL_NONE}, _depot_gfx_NW }
};


/* Sprite layout for level crossings. The SpriteIDs are actually offsets
 * from the base SpriteID returned from the NewGRF sprite resolver. */
static const DrawTileSeqStruct _crossing_layout_ALL[] = {
	TILE_SEQ_LINE2(2, PAL_NONE,  0,  0, 3, 3)
	TILE_SEQ_LINE2(4, PAL_NONE,  0, 13, 3, 3)
	TILE_SEQ_LINE2(6, PAL_NONE, 13,  0, 3, 3)
	TILE_SEQ_LINE2(8, PAL_NONE, 13, 13, 3, 3)
	TILE_SEQ_END()
};

static const DrawTileSprites _crossing_layout = {
	{0, PAL_NONE}, _crossing_layout_ALL
};

static const DrawTileSeqStruct _crossing_layout_SW_ALL[] = {
	TILE_SEQ_LINE2(6, PAL_NONE, 13,  0, 3, 3)
	TILE_SEQ_LINE2(8, PAL_NONE, 13, 13, 3, 3)
	TILE_SEQ_END()
};

static const DrawTileSprites _crossing_layout_SW = {
	{0, PAL_NONE}, _crossing_layout_SW_ALL
};

static const DrawTileSeqStruct _crossing_layout_NW_ALL[] = {
	TILE_SEQ_LINE2(2, PAL_NONE,  0,  0, 3, 3)
	TILE_SEQ_LINE2(6, PAL_NONE, 13,  0, 3, 3)
	TILE_SEQ_END()
};

static const DrawTileSprites _crossing_layout_NW = {
	{0, PAL_NONE}, _crossing_layout_NW_ALL
};

static const DrawTileSeqStruct _crossing_layout_NE_ALL[] = {
	TILE_SEQ_LINE2(2, PAL_NONE,  0,  0, 3, 3)
	TILE_SEQ_LINE2(4, PAL_NONE,  0, 13, 3, 3)
	TILE_SEQ_END()
};

static const DrawTileSprites _crossing_layout_NE = {
	{0, PAL_NONE}, _crossing_layout_NE_ALL
};

static const DrawTileSeqStruct _crossing_layout_SE_ALL[] = {
	TILE_SEQ_LINE2(4, PAL_NONE,  0, 13, 3, 3)
	TILE_SEQ_LINE2(8, PAL_NONE, 13, 13, 3, 3)
	TILE_SEQ_END()
};

static const DrawTileSprites _crossing_layout_SE = {
	{0, PAL_NONE}, _crossing_layout_SE_ALL
};

#undef TILE_SEQ_LINE
#undef TILE_SEQ_LINE2
#undef TILE_SEQ_END
