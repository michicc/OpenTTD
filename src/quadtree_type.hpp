/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file quadtree_type.hpp Templated quad tree to store data keyed by map areas. */

#ifndef QUADTREE_TYPE_HPP
#define QUADTREE_TYPE_HPP

#include "core/mem_func.hpp"
#include "tilearea_type.h"


/**
 *
 * @tparam T The type of the stored items.
 * @tparam N Minimal grid size.
 */
template <typename T, int N>
class QuadTree {
	int xc;
	int yc;
	int len;
	QuadTree *nodes[4];

public:
	T data;

	QuadTree(int xc, int yc, int len) : xc(xc), yc(yc), len(len)
	{

	}

	/**
	 *
	 * @param
	 * @return
	 */
	const QuadTree *Get(int x, int y) const
	{
		int i = x < this->xc ? 0 : 2;
		if (y >= this->yc) i++;

		if (this->nodes[i] != NULL) return nodes[i]->Get(x, y);
		return this;
	}

	/**
	 *
	 * @param
	 * @return
	 */
	QuadTree *Get(int x, int y)
	{
		int i = x < this->xc ? 0 : 2;
		if (y >= this->yc) i++;

		if (this->nodes[i] != NULL) return nodes[i]->Get(x, y);
		return this;
	}

	/**
	 *
	 * @param
	 * @return
	 */
	void Subdivide()
	{
		int half = len / 2;
		if (half < N) return;

		this->nodes[0] = new QuadTree(this->xc / 2,        this->yc / 2,        half);
		this->nodes[1] = new QuadTree(this->xc / 2,        this->yc / 2 + half, half);
		this->nodes[2] = new QuadTree(this->xc / 2 + half, this->yc / 2,        half);
		this->nodes[3] = new QuadTree(this->xc / 2 + half, this->yc / 2 + half, half);

		/* Copy node data. */
		for (int i = 0; i < 4; i++) this->nodes[i]->data = this->data;
	}
};

#endif /* QUADTREE_TYPE_HPP */
