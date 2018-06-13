/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

GSLog.Info("1.6 API compatibility in effect.");

/* 1.9 adds a vehicle type parameter. */
GSBridge._GetName <- GSBridge.GetName;
GSBridge.GetName <- function(bridge_id)
{
	return GSBridge._GetName(bridge_id, GSVehicle.VT_RAIL);
}

GSRail._GetRailType <- GSRail.GetRailType;
GSRail.GetRailType <- function(tile)
{
	local tile_x = GSMap.GetTileX(tile);
	local tile_y = GSMap.GetTileY(tile);
	local dirs = [[-1, 0], [0, -1], [1, 0], [0, 1]];

	foreach (d in dirs) {
		local rt = GSRail._GetRailType(tile, GSMap.GetTileIndex(tile_x + d[0], tile_y + d[1]));
		if (rt != GSRail.RAILTYPE_INVALID) return rt;
	}
	return GSRail.RAILTYPE_INVALID;
}
