/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

AILog.Info("1.10 API compatibility in effect.");

AIRail._GetRailType <- AIRail.GetRailType;
AIRail.GetRailType <- function(tile)
{
	local tile_x = AIMap.GetTileX(tile);
	local tile_y = AIMap.GetTileY(tile);
	local dirs = [[-1, 0], [0, -1], [1, 0], [0, 1]];

	foreach (d in dirs) {
		local rt = AIRail._GetRailType(tile, AIMap.GetTileIndex(tile_x + d[0], tile_y + d[1]));
		if (rt != AIRail.RAILTYPE_INVALID) return rt;
	}
	return AIRail.RAILTYPE_INVALID;
}
