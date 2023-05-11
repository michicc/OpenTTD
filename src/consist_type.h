/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file consist_type.h Types related to vehicle consists. */

#ifndef CONSIST_TYPE_H
#define CONSIST_TYPE_H

/** The type all our consist IDs have. */
typedef uint32_t ConsistID;

class Consist;

/** Constant representing a non-existing consist. */
static const ConsistID INVALID_CONSIST = 0xFFFFFF;

#endif /* CONSIST_TYPE_H */
