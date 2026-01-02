/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file cargodest_base.h GUI functions related to the cargodest GUI. */

#ifndef CARGODEST_GUI_H
#define CARGODEST_GUI_H

#include "cargodest_base.h"
#include "core/geometry_type.hpp"
#include "sortlist_type.h"
#include "strings_type.h"

/** Helper encapsulating a #CargoLink. */
struct GUICargoDemandLink {
	CargoType cargo_type; ///< Cargo type of this link.
	const CargoDemandLink *link; ///< Pointer to the link.

	GUICargoDemandLink(CargoType ct, const CargoDemandLink *link) : cargo_type(ct), link(link) {}
};

/** Sorted list of demand destinations for displaying. */
class CargoDestinationList {
	const CargoSourceSink *obj; ///< The object which destinations are displayed.
	GUIList<GUICargoDemandLink> link_list; ///< Sorted list of destinations.

	void RebuildList();
	void SortList();
	std::string GetDisplayString(const GUICargoDemandLink &link) const;

public:
	CargoDestinationList(const CargoSourceSink *css);

	void InvalidateData();
	void Resort();
	void DrawList(const Rect &r, int pos) const;

	void OnClick(int pos) const;

	Dimension GetListSize(bool town) const;

	/**
	 * Get total amount of lines displayed in the list.
	 * @return Number of lines.
	 */
	size_t GetLineCount() const
	{
		return 1 + link_list.size();
	}
};

#endif /* CARGODEST_GUI_H */
