/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file map.cpp Base functions related to the map and distances on them. */

#include "stdafx.h"
#include "debug.h"
#include "core/alloc_func.hpp"
#include "water_map.h"
#include "error_func.h"
#include "string_func.h"
#include "pathfinder/water_regions.h"

#include "safeguards.h"

/* static */ uint Map::log_x;     ///< 2^_map_log_x == _map_size_x
/* static */ uint Map::log_y;     ///< 2^_map_log_y == _map_size_y
/* static */ uint Map::size_x;    ///< Size of the map along the X
/* static */ uint Map::size_y;    ///< Size of the map along the Y
/* static */ uint Map::size;      ///< The number of tiles on the map
/* static */ uint Map::tile_mask; ///< _map_size - 1 (to mask the mapsize)

/* static */ std::vector<std::vector<Map::TileBase>> Map::base_tiles{};
/* static */ std::vector<uint16_t> Map::offsets{};



/**
 * (Re)allocates a map with the given dimension
 * @param size_x the width of the map along the NE/SW edge
 * @param size_y the 'height' of the map along the SE/NW edge
 */
/* static */ void Map::Allocate(uint size_x, uint size_y)
{
	/* Make sure that the map size is within the limits and that
	 * size of both axes is a power of 2. */
	if (!IsInsideMM(size_x, MIN_MAP_SIZE, MAX_MAP_SIZE + 1) ||
			!IsInsideMM(size_y, MIN_MAP_SIZE, MAX_MAP_SIZE + 1) ||
			(size_x & (size_x - 1)) != 0 ||
			(size_y & (size_y - 1)) != 0) {
		FatalError("Invalid map size");
	}

	Debug(map, 1, "Allocating map of size {}x{}", size_x, size_y);

	Map::log_x = FindFirstBit(size_x);
	Map::log_y = FindFirstBit(size_y);
	Map::size_x = size_x;
	Map::size_y = size_y;
	Map::size = size_x * size_y;
	Map::tile_mask = Map::size - 1;

	/* Allocate tiles. */
	Map::base_tiles.clear();
	Map::base_tiles.resize(size_y, std::vector<Map::TileBase>{ size_x });
	/* Allocate offset array for each map line. */
	Map::offsets.clear();
	Map::offsets.resize(size_x * size_y);
	for (uint i = 0; i < size_x * size_y; i++) {
		Map::offsets[i] = i & (size_x - 1);
	}

	AllocateWaterRegions();
}

/**
 * Get raw tile count.
 */
/* static */ size_t Map::GetTotalTileCount()
{
	return std::accumulate(Map::base_tiles.begin(), Map::base_tiles.end(), size_t{0}, [](size_t s, const std::vector<Map::TileBase> &t) { return s + t.size(); });
}

/**
 * Add a new tile to the map.
 * @param index Tile index where to add a tile to.
 * @param type Type of the new tile.
 * @param insert_after Associated sub-tile to insert the new tile after, or an invalid tile to insert after the last sub-tile.
 * @param raw_alloc If true, associated tile flags and tile data will not be initialized. Used for saveload code.
 * @return Newly added tile.
 * @pre \c insert_after is either invalid or associated with the tile index.
 */
/* static */ Tile Tile::New(TileIndex index, TileType type, Tile insert_after, bool raw_alloc)
{
	[[maybe_unused]] auto check_tile = [](TileIndex i, Tile check) -> bool {
		/* Check if the tile belongs to the tile index. */
		for (Tile t = i; t.IsValid(); ++t) {
			if (t == check) return true;
		}
		return false;
	};
	assert(!insert_after.IsValid() || check_tile(index, insert_after));

	/* Insert at the end if nothing is specified. */
	if (!insert_after.IsValid()) {
		insert_after = index;
		while (insert_after.HasAssociated()) ++insert_after;
	}

	bool has_next = insert_after.HasAssociated();
	if (!raw_alloc) insert_after.SetAssociated(true);

	/* Fixup tile offsets. */
	uint count = Map::size_x - TileX(index);
	for (uint i = 1; i < count; i++) {
		Map::offsets[index.base() + i]++;
	}

	/* Insert new tile. */
	auto &line = Map::base_tiles[TileY(index)];
	Tile new_tile(std::addressof(*line.emplace(line.begin() + (insert_after.tile - line.data()) + 1)));

	if (!raw_alloc) {
		SetTileType(new_tile, type);
		if (has_next) new_tile.SetAssociated(true);
	}

	return new_tile;
}

/**
 * Remove a tile from the map.
 * @param index Tile index from where to remove a tile.
 * @param to_remove Associated sub-tile to remove.
 * @return Next associated tile after the removed tile if present or an invalid tile otherwise.
 * @pre Tile(index).HasAssociated()
 * @pre Tile is associated with this tile index.
 */
/* static */ Tile Tile::Remove(TileIndex index, Tile to_remove)
{
	assert(Tile(index).HasAssociated()); // Can't remove the last tile from a tile index.

	for (Tile cur_tile = index; cur_tile.HasAssociated(); ++cur_tile) {
		if (cur_tile.tile + 1 == to_remove.tile) {
			/* Copy associated tile flag from tile to be removed. */
			bool has_next = to_remove.HasAssociated();
			cur_tile.SetAssociated(has_next);
			/* Remove tile. */
			auto &line = Map::base_tiles[TileY(index)];
			auto next = line.erase(line.begin() + (to_remove.tile - line.data()));

			/* Fix-up tile offsets. */
			uint count = Map::size_x - TileX(index);
			for (uint i = 1; i < count; i++) {
				Map::offsets[index.base() + i]--;
			}

			/* Return next associated tile after the removed tile (if there is one). */
			return {has_next ? std::addressof(*next) : nullptr};
		}
	}

	/* Tile wasn't actually part of this tile index. */
	NOT_REACHED();
}

#ifdef _DEBUG
TileIndex TileAdd(TileIndex tile, TileIndexDiff offset)
{
	int dx = offset & Map::MaxX();
	if (dx >= (int)Map::SizeX() / 2) dx -= Map::SizeX();
	int dy = (offset - dx) / (int)Map::SizeX();

	uint32_t x = TileX(tile) + dx;
	uint32_t y = TileY(tile) + dy;

	assert(x < Map::SizeX());
	assert(y < Map::SizeY());
	assert(TileXY(x, y) == Map::WrapToMap(tile + offset));

	return TileXY(x, y);
}
#endif

/**
 * This function checks if we add addx/addy to tile, if we
 * do wrap around the edges. For example, tile = (10,2) and
 * addx = +3 and addy = -4. This function will now return
 * INVALID_TILE, because the y is wrapped. This is needed in
 * for example, farmland. When the tile is not wrapped,
 * the result will be tile + TileDiffXY(addx, addy)
 *
 * @param tile the 'starting' point of the adding
 * @param addx the amount of tiles in the X direction to add
 * @param addy the amount of tiles in the Y direction to add
 * @return translated tile, or INVALID_TILE when it would've wrapped.
 */
TileIndex TileAddWrap(TileIndex tile, int addx, int addy)
{
	uint x = TileX(tile) + addx;
	uint y = TileY(tile) + addy;

	/* Disallow void tiles at the north border. */
	if ((x == 0 || y == 0) && _settings_game.construction.freeform_edges) return INVALID_TILE;

	/* Are we about to wrap? */
	if (x >= Map::MaxX() || y >= Map::MaxY()) return INVALID_TILE;

	return TileXY(x, y);
}

/** 'Lookup table' for tile offsets given an Axis */
extern const TileIndexDiffC _tileoffs_by_axis[] = {
	{ 1,  0}, ///< AXIS_X
	{ 0,  1}, ///< AXIS_Y
};

/** 'Lookup table' for tile offsets given a DiagDirection */
extern const TileIndexDiffC _tileoffs_by_diagdir[] = {
	{-1,  0}, ///< DIAGDIR_NE
	{ 0,  1}, ///< DIAGDIR_SE
	{ 1,  0}, ///< DIAGDIR_SW
	{ 0, -1}  ///< DIAGDIR_NW
};

/** 'Lookup table' for tile offsets given a Direction */
extern const TileIndexDiffC _tileoffs_by_dir[] = {
	{-1, -1}, ///< DIR_N
	{-1,  0}, ///< DIR_NE
	{-1,  1}, ///< DIR_E
	{ 0,  1}, ///< DIR_SE
	{ 1,  1}, ///< DIR_S
	{ 1,  0}, ///< DIR_SW
	{ 1, -1}, ///< DIR_W
	{ 0, -1}  ///< DIR_NW
};

/**
 * Gets the Manhattan distance between the two given tiles.
 * The Manhattan distance is the sum of the delta of both the
 * X and Y component.
 * Also known as L1-Norm
 * @param t0 the start tile
 * @param t1 the end tile
 * @return the distance
 */
uint DistanceManhattan(TileIndex t0, TileIndex t1)
{
	const uint dx = Delta(TileX(t0), TileX(t1));
	const uint dy = Delta(TileY(t0), TileY(t1));
	return dx + dy;
}


/**
 * Gets the 'Square' distance between the two given tiles.
 * The 'Square' distance is the square of the shortest (straight line)
 * distance between the two tiles.
 * Also known as euclidian- or L2-Norm squared.
 * @param t0 the start tile
 * @param t1 the end tile
 * @return the distance
 */
uint DistanceSquare(TileIndex t0, TileIndex t1)
{
	const int dx = TileX(t0) - TileX(t1);
	const int dy = TileY(t0) - TileY(t1);
	return dx * dx + dy * dy;
}


/**
 * Gets the biggest distance component (x or y) between the two given tiles.
 * Also known as L-Infinity-Norm.
 * @param t0 the start tile
 * @param t1 the end tile
 * @return the distance
 */
uint DistanceMax(TileIndex t0, TileIndex t1)
{
	const uint dx = Delta(TileX(t0), TileX(t1));
	const uint dy = Delta(TileY(t0), TileY(t1));
	return std::max(dx, dy);
}


/**
 * Gets the biggest distance component (x or y) between the two given tiles
 * plus the Manhattan distance, i.e. two times the biggest distance component
 * and once the smallest component.
 * @param t0 the start tile
 * @param t1 the end tile
 * @return the distance
 */
uint DistanceMaxPlusManhattan(TileIndex t0, TileIndex t1)
{
	const uint dx = Delta(TileX(t0), TileX(t1));
	const uint dy = Delta(TileY(t0), TileY(t1));
	return dx > dy ? 2 * dx + dy : 2 * dy + dx;
}

/**
 * Param the minimum distance to an edge
 * @param tile the tile to get the distance from
 * @return the distance from the edge in tiles
 */
uint DistanceFromEdge(TileIndex tile)
{
	const uint xl = TileX(tile);
	const uint yl = TileY(tile);
	const uint xh = Map::SizeX() - 1 - xl;
	const uint yh = Map::SizeY() - 1 - yl;
	const uint minl = std::min(xl, yl);
	const uint minh = std::min(xh, yh);
	return std::min(minl, minh);
}

/**
 * Gets the distance to the edge of the map in given direction.
 * @param tile the tile to get the distance from
 * @param dir the direction of interest
 * @return the distance from the edge in tiles
 */
uint DistanceFromEdgeDir(TileIndex tile, DiagDirection dir)
{
	switch (dir) {
		case DIAGDIR_NE: return             TileX(tile) - (_settings_game.construction.freeform_edges ? 1 : 0);
		case DIAGDIR_NW: return             TileY(tile) - (_settings_game.construction.freeform_edges ? 1 : 0);
		case DIAGDIR_SW: return Map::MaxX() - TileX(tile) - 1;
		case DIAGDIR_SE: return Map::MaxY() - TileY(tile) - 1;
		default: NOT_REACHED();
	}
}

/**
 * Function performing a search around a center tile and going outward, thus in circle.
 * Although it really is a square search...
 * Every tile will be tested by means of the callback function proc,
 * which will determine if yes or no the given tile meets criteria of search.
 * @param tile to start the search from. Upon completion, it will return the tile matching the search
 * @param size: number of tiles per side of the desired search area
 * @param proc: callback testing function pointer.
 * @param user_data to be passed to the callback function. Depends on the implementation
 * @return result of the search
 * @pre proc != nullptr
 * @pre size > 0
 */
bool CircularTileSearch(TileIndex *tile, uint size, TestTileOnSearchProc proc, void *user_data)
{
	assert(proc != nullptr);
	assert(size > 0);

	if (size % 2 == 1) {
		/* If the length of the side is uneven, the center has to be checked
		 * separately, as the pattern of uneven sides requires to go around the center */
		if (proc(*tile, user_data)) return true;

		/* If tile test is not successful, get one tile up,
		 * ready for a test in first circle around center tile */
		*tile = TileAddByDir(*tile, DIR_N);
		return CircularTileSearch(tile, size / 2, 1, 1, proc, user_data);
	} else {
		return CircularTileSearch(tile, size / 2, 0, 0, proc, user_data);
	}
}

/**
 * Generalized circular search allowing for rectangles and a hole.
 * Function performing a search around a center rectangle and going outward.
 * The center rectangle is left out from the search. To do a rectangular search
 * without a hole, set either h or w to zero.
 * Every tile will be tested by means of the callback function proc,
 * which will determine if yes or no the given tile meets criteria of search.
 * @param tile to start the search from. Upon completion, it will return the tile matching the search.
 *  This tile should be directly north of the hole (if any).
 * @param radius How many tiles to search outwards. Note: This is a radius and thus different
 *                from the size parameter of the other CircularTileSearch function, which is a diameter.
 * @param w the width of the inner rectangle
 * @param h the height of the inner rectangle
 * @param proc callback testing function pointer.
 * @param user_data to be passed to the callback function. Depends on the implementation
 * @return result of the search
 * @pre proc != nullptr
 * @pre radius > 0
 */
bool CircularTileSearch(TileIndex *tile, uint radius, uint w, uint h, TestTileOnSearchProc proc, void *user_data)
{
	assert(proc != nullptr);
	assert(radius > 0);

	uint x = TileX(*tile) + w + 1;
	uint y = TileY(*tile);

	const uint extent[DIAGDIR_END] = { w, h, w, h };

	for (uint n = 0; n < radius; n++) {
		for (DiagDirection dir = DIAGDIR_BEGIN; dir < DIAGDIR_END; dir++) {
			/* Is the tile within the map? */
			for (uint j = extent[dir] + n * 2 + 1; j != 0; j--) {
				if (x < Map::SizeX() && y < Map::SizeY()) {
					TileIndex t = TileXY(x, y);
					/* Is the callback successful? */
					if (proc(t, user_data)) {
						/* Stop the search */
						*tile = t;
						return true;
					}
				}

				/* Step to the next 'neighbour' in the circular line */
				x += _tileoffs_by_diagdir[dir].x;
				y += _tileoffs_by_diagdir[dir].y;
			}
		}
		/* Jump to next circle to test */
		x += _tileoffs_by_dir[DIR_W].x;
		y += _tileoffs_by_dir[DIR_W].y;
	}

	*tile = INVALID_TILE;
	return false;
}

/**
 * Finds the distance for the closest tile with water/land given a tile
 * @param tile  the tile to find the distance too
 * @param water whether to find water or land
 * @return distance to nearest water (max 0x7F) / land (max 0x1FF; 0x200 if there is no land)
 */
uint GetClosestWaterDistance(TileIndex tile, bool water)
{
	if (HasTileWaterGround(tile) == water) return 0;

	uint max_dist = water ? 0x7F : 0x200;

	int x = TileX(tile);
	int y = TileY(tile);

	uint max_x = Map::MaxX();
	uint max_y = Map::MaxY();
	uint min_xy = _settings_game.construction.freeform_edges ? 1 : 0;

	/* go in a 'spiral' with increasing manhattan distance in each iteration */
	for (uint dist = 1; dist < max_dist; dist++) {
		/* next 'diameter' */
		y--;

		/* going counter-clockwise around this square */
		for (DiagDirection dir = DIAGDIR_BEGIN; dir < DIAGDIR_END; dir++) {
			static const int8_t ddx[DIAGDIR_END] = { -1,  1,  1, -1};
			static const int8_t ddy[DIAGDIR_END] = {  1,  1, -1, -1};

			int dx = ddx[dir];
			int dy = ddy[dir];

			/* each side of this square has length 'dist' */
			for (uint a = 0; a < dist; a++) {
				/* MP_VOID tiles are not checked (interval is [min; max) for IsInsideMM())*/
				if (IsInsideMM(x, min_xy, max_x) && IsInsideMM(y, min_xy, max_y)) {
					TileIndex t = TileXY(x, y);
					if (HasTileWaterGround(t) == water) return dist;
				}
				x += dx;
				y += dy;
			}
		}
	}

	if (!water) {
		/* no land found - is this a water-only map? */
		for (const auto t : Map::Iterate()) {
			if (!IsTileType(t, MP_VOID) && !IsTileType(t, MP_WATER)) return 0x1FF;
		}
	}

	return max_dist;
}
