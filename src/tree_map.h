/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tree_map.h Map accessors for tree tiles. */

#ifndef TREE_MAP_H
#define TREE_MAP_H

#include "tile_map.h"
#include "water_map.h"

/**
 * List of tree types along all landscape types.
 *
 * This enumeration contains a list of the different tree types along
 * all landscape types. The values for the enumerations may be used for
 * offsets from the grfs files. These points to the start of
 * the tree list for a landscape. See the TREE_COUNT_* enumerations
 * for the amount of different trees for a specific landscape.
 */
enum TreeType {
	TREE_TEMPERATE    = 0x00, ///< temperate tree
	TREE_SUB_ARCTIC   = 0x0C, ///< tree on a sub_arctic landscape
	TREE_RAINFOREST   = 0x14, ///< tree on the 'green part' on a sub-tropical map
	TREE_CACTUS       = 0x1B, ///< a cactus for the 'desert part' on a sub-tropical map
	TREE_SUB_TROPICAL = 0x1C, ///< tree on a sub-tropical map, non-rainforest, non-desert
	TREE_TOYLAND      = 0x20, ///< tree on a toyland map
	TREE_INVALID      = 0xFF, ///< An invalid tree
};

/* Counts the number of tree types for each landscape.
 *
 * This list contains the counts of different tree types for each landscape. This list contains
 * 5 entries instead of 4 (as there are only 4 landscape types) as the sub tropic landscape
 * has two types of area, one for normal trees and one only for cacti.
 */
static const uint TREE_COUNT_TEMPERATE    = TREE_SUB_ARCTIC - TREE_TEMPERATE;    ///< number of tree types on a temperate map.
static const uint TREE_COUNT_SUB_ARCTIC   = TREE_RAINFOREST - TREE_SUB_ARCTIC;   ///< number of tree types on a sub arctic map.
static const uint TREE_COUNT_RAINFOREST   = TREE_CACTUS     - TREE_RAINFOREST;   ///< number of tree types for the 'rainforest part' of a sub-tropic map.
static const uint TREE_COUNT_SUB_TROPICAL = TREE_TOYLAND    - TREE_SUB_TROPICAL; ///< number of tree types for the 'sub-tropic part' of a sub-tropic map.
static const uint TREE_COUNT_TOYLAND      = 9;                                   ///< number of tree types on a toyland map.


/**
 * Returns the treetype of a tile.
 *
 * This function returns the treetype of a given tile. As there are more
 * possible treetypes for a tile in a game as the enumeration #TreeType defines
 * this function may be return a value which isn't catch by an entry of the
 * enumeration #TreeType. But there is no problem known about it.
 *
 * @param t The tile to get the treetype from
 * @return The treetype of the given tile with trees
 * @pre Tile t must be of type MP_TREES
 */
static inline TreeType GetTreeType(const Tile *t)
{
	assert(IsTileType(t, MP_TREES));
	return (TreeType)t->m3;
}

/**
	SetWaterClass(t, g == TREE_GROUND_SHORE ? WATER_CLASS_SEA : WATER_CLASS_INVALID);
 * Returns the number of trees on a tile.
 *
 * This function returns the number of trees of a tile (1-4).
 * The tile must be contains at least one tree or be more specific: it must be
 * of type MP_TREES.
 *
 * @param t The index to get the number of trees
 * @return The number of trees (1-4)
 * @pre Tile must be of type MP_TREES
 */
static inline uint GetTreeCount(const Tile *t)
{
	assert(IsTileType(t, MP_TREES));
	return GB(t->m5, 6, 2) + 1;
}

/**
 * Add a amount to the tree-count value of a tile with trees.
 *
 * This function add a value to the tree-count value of a tile. This
 * value may be negative to reduce the tree-counter. If the resulting
 * value reach 0 it doesn't get converted to a "normal" tile.
 *
 * @param t The tile to change the tree amount
 * @param c The value to add (or reduce) on the tree-count value
 * @pre Tile must be of type MP_TREES
 */
static inline void AddTreeCount(Tile *t, int c)
{
	assert(IsTileType(t, MP_TREES)); // XXX incomplete
	t->m5 += c << 6;
}

/**
 * Returns the tree growth status.
 *
 * This function returns the tree growth status of a tile with trees.
 *
 * @param t The tile to get the tree growth status
 * @return The tree growth status
 * @pre Tile must be of type MP_TREES
 */
static inline uint GetTreeGrowth(const Tile *t)
{
	assert(IsTileType(t, MP_TREES));
	return GB(t->m5, 0, 3);
}

/**
 * Add a value to the tree growth status.
 *
 * This function adds a value to the tree grow status of a tile.
 *
 * @param t The tile to add the value on
 * @param a The value to add on the tree growth status
 * @pre Tile must be of type MP_TREES
 */
static inline void AddTreeGrowth(Tile *t, int a)
{
	assert(IsTileType(t, MP_TREES)); // XXX incomplete
	t->m5 += a;
}

/**
 * Sets the tree growth status of a tile.
 *
 * This function sets the tree growth status of a tile directly with
 * the given value.
 *
 * @param t The tile to change the tree growth status
 * @param g The new value
 * @pre Tile must be of type MP_TREES
 */
static inline void SetTreeGrowth(Tile *t, uint g)
{
	assert(IsTileType(t, MP_TREES)); // XXX incomplete
	SB(t->m5, 0, 3, g);
}

/**
 * Get the tick counter of a tree tile.
 *
 * Returns the saved tick counter of a given tile.
 *
 * @param t The tile to get the counter value from
 * @pre Tile must be of type MP_TREES
 */
static inline uint GetTreeCounter(const Tile *t)
{
	assert(IsTileType(t, MP_TREES));
	return GB(t->m2, 0, 4);
}

/**
 * Add a value on the tick counter of a tree-tile
 *
 * This function adds a value on the tick counter of a tree-tile.
 *
 * @param t The tile to add the value on
 * @param a The value to add on the tick counter
 * @pre Tile must be of type MP_TREES
 */
static inline void AddTreeCounter(Tile *t, int a)
{
	assert(IsTileType(t, MP_TREES)); // XXX incomplete
	t->m2 += a;
}

/**
 * Set the tick counter for a tree-tile
 *
 * This function sets directly the tick counter for a tree-tile.
 *
 * @param t The tile to set the tick counter
 * @param c The new tick counter value
 * @pre Tile must be of type MP_TREES
 */
static inline void SetTreeCounter(Tile *t, uint c)
{
	assert(IsTileType(t, MP_TREES)); // XXX incomplete
	SB(t->m2, 0, 4, c);
}

/**
 * Make a tree-tile.
 *
 * This functions change the tile to a tile with trees and all informations which belongs to it.
 *
 * @param t The tile to make a tree-tile from
 * @param type The type of the tree
 * @param count the number of trees
 * @param growth the growth status
 */
static inline Tile *MakeTree(TileIndex tile, TreeType type, uint count, uint growth)
{
	Tile *t = _m.NewTile(tile, MP_TREES);

	SetTileOwner(t, OWNER_NONE);
	t->m3 = type;
	t->m5 = count << 6 | growth;

	return t;
}

#endif /* TREE_MAP_H */
