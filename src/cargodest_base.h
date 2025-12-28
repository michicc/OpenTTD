/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file cargodest_base.h Classes and types for entities having cargo destinations. */

#ifndef CARGODEST_BASE_H
#define CARGODEST_BASE_H

#include "source_type.h"
#include "tile_type.h"

/** An entity producing or accepting cargo with a destination. */
struct CargoSourceSink {
	/** Get the source of this entity. */
	virtual Source ToSource() const = 0;
	/** Get the base map coordinate of this entity. */
	virtual TileIndex GetXY() const = 0;
};

#endif /* CARGODEST_BASE_H */
