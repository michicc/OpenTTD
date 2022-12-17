/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tilematrix_type.hpp Template for storing a value per area of the map. */

#ifndef TILEMATRIX_TYPE_HPP
#define TILEMATRIX_TYPE_HPP

#include "core/alloc_func.hpp"
#include "tilearea_type.h"
#include <vector>

/**
 * A simple matrix that stores one value per N*N square of the map.
 * Storage is only allocated for the part of the map that has values
 * assigned.
 *
 * @tparam T The type of the stored items.
 * @tparam N Grid size.
 */
template <typename T, uint N>
class TileMatrix {
	TileArea area{};     ///< Area covered by the matrix.
	std::vector<T> data; ///< Data array.

	/** Allocates space for a new tile in the matrix.
	 * @param tile Tile to add.
	 */
	void AllocateStorage(TileIndex tile)
	{
		uint old_left = TileX(this->area.tile) / N;
		uint old_top  = TileY(this->area.tile) / N;
		uint old_w    = this->area.w / N;
		uint old_h    = this->area.h / N;

		/* Add the square the tile is in to the tile area. We do this
		 * by adding top-left and bottom-right of the square. */
		uint grid_x = (TileX(tile) / N) * N;
		uint grid_y = (TileY(tile) / N) * N;
		this->area.Add(TileXY(grid_x, grid_y));
		this->area.Add(TileXY(grid_x + N - 1, grid_y + N - 1));

		/* Allocate new storage. */
		std::vector<T> new_data(this->area.w / N * this->area.h / N);

		if (old_w > 0) {
			/* Copy old data if present. */
			uint offs_x = old_left - TileX(this->area.tile) / N;
			uint offs_y = old_top  - TileY(this->area.tile) / N;

			for (uint row = 0; row < old_h; row++) {
				std::copy_n(std::make_move_iterator(this->data.begin() + row * old_w), old_w, new_data.begin() + (row + offs_y) * this->area.w / N + offs_x);
			}
		}

		this->data = std::move(new_data);
	}

	friend class SlTownAcceptanceMatrix;

public:
	static const uint GRID = N;

	/**
	 * Get the total covered area.
	 * @return The area covered by the matrix.
	 */
	const TileArea& GetArea() const
	{
		return this->area;
	}

	/**
	 * Get the area of the matrix square that contains a specific tile.
	 * @param tile The tile to get the map area for.
	 * @param extend Extend the area by this many squares on all sides.
	 * @return Tile area containing the tile.
	 */
	static TileArea GetAreaForTile(TileIndex tile, uint extend = 0)
	{
		uint tile_x = (TileX(tile) / N) * N;
		uint tile_y = (TileY(tile) / N) * N;

		return TileArea(TileXY(tile_x, tile_y), N, N).Expanded(extend * N);
	}

	/**
	 * Get the area of the matrix square that contains a specific tile area.
	 * @param area The tile area to get the map area for.
	 * @param extend Extend the area by this many squares on all sides.
	 * @return Tile area containing the area.
	 */
	static TileArea GetAreaForTiles(const TileArea &area, uint extend = 0)
	{
		uint tile_x = (TileX(area.tile) / N) * N;
		uint tile_y = (TileY(area.tile) / N) * N;

		uint tile_x_2 = ((TileX(area.tile) + area.w - 1) / N) * N + N - 1;
		uint tile_y_2 = ((TileY(area.tile) + area.h - 1) / N) * N + N - 1;

		return TileArea(TileXY(tile_x, tile_y), TileXY(tile_x_2, tile_y_2)).Expanded(extend * N);
	}

	/**
	 * Check if a tile is the primary tile for a grid square.
	 * @param Tile to check.
	 * @return True if the tile is the origin of a grid square.
	 */
	static inline bool IsOnGrid(TileIndex tile)
	{
		return TileX(tile) % GRID == 0 && TileY(tile) % GRID == 0;
	}

	/**
	 * Extend the coverage area to include a tile.
	 * @param tile The tile to include.
	 */
	void Add(TileIndex tile)
	{
		if (!this->area.Contains(tile)) {
			this->AllocateStorage(tile);
		}
	}

	/**
	 * Extend the coverage area to include a tile area.
	 * @param area The area to include.
	 */
	void Add(const TileArea &area)
	{
		if (!this->area.Contains(area)) {
			this->Add(area.tile);
			this->Add(TILE_ADDXY(area.tile, area.w, area.h));
		}
	}

	/**
	 * Get the value associated to a tile index.
	 * @param tile The tile to get the value for.
	 * @return Pointer to the value.
	 */
	T *Get(TileIndex tile)
	{
		this->Add(tile);

		tile -= this->area.tile;
		uint x = TileX(tile) / N;
		uint y = TileY(tile) / N;

		return &this->data[y * this->area.w / N + x];
	}

	/** Array access operator, see #Get. */
	inline T &operator[](TileIndex tile)
	{
		return *this->Get(tile);
	}
};

#endif /* TILEMATRIX_TYPE_HPP */
