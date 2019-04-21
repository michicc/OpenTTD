/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cargodest_gui.cpp GUI for cargo destinations. */

#include "stdafx.h"
#include "window_gui.h"
#include "gfx_func.h"
#include "strings_func.h"
#include "cargodest_base.h"
#include "cargodest_gui.h"
#include "town.h"
#include "industry.h"
#include "string_func.h"
#include "gui.h"
#include "viewport_func.h"

#include "table/strings.h"

static const CargoSourceSink *_cur_cargo_source;

bool CargoLinkSorter(const GUICargoLink &a, const GUICargoLink &b)
{
	/* Sort by cargo type. */
	if (a.cid != b.cid) return a.cid < b.cid;

	/* Sort unspecified destination links always last. */
	if (a.link->dest == nullptr) return false;
	if (b.link->dest == nullptr) return true;

	/* Sort link with the current source as destination first. */
	if (a.link->dest == _cur_cargo_source) return true;
	if (b.link->dest == _cur_cargo_source) return false;

	/* Sort towns before industries. */
	if (a.link->dest->GetType() != b.link->dest->GetType()) {
		return a.link->dest->GetType() < b.link->dest->GetType();
	}

	/* Sort by name. */
	static const CargoLink *last_b = nullptr;
	static char last_name[128];

	char name[128];
	SetDParam(0, a.link->dest->GetID());
	GetString(name, a.link->dest->GetType() == ST_TOWN ? STR_TOWN_NAME : STR_INDUSTRY_NAME, lastof(name));

	/* Cache name lookup of 'b', as the sorter is often called
	 * multiple times with the same 'b'. */
	if (b.link != last_b) {
		last_b = b.link;

		SetDParam(0, b.link->dest->GetID());
		GetString(last_name, b.link->dest->GetType() == ST_TOWN ? STR_TOWN_NAME : STR_INDUSTRY_NAME, lastof(last_name));
	}

	return strcmp(name, last_name) < 0;
}

CargoDestinationList::CargoDestinationList(const CargoSourceSink *o) : obj(o)
{
	this->InvalidateData();
}

/** Rebuild the link list from the source object. */
void CargoDestinationList::RebuildList()
{
	if (!this->link_list.NeedRebuild()) return;

	this->link_list.clear();
	for (CargoID i = 0; i < lengthof(this->obj->cargo_links); i++) {
		for (const CargoLink &l : this->obj->cargo_links[i]) {
			this->link_list.emplace_back(i, &l);
		}
	}

	this->link_list.RebuildDone();
}

/** Sort the link list. */
void CargoDestinationList::SortList()
{
	_cur_cargo_source = this->obj;
	this->link_list.Sort(&CargoLinkSorter);
}

/** Rebuild the list, e.g. when a new cargo link was added. */
void CargoDestinationList::InvalidateData()
{
	this->link_list.ForceRebuild();
	this->RebuildList();
	this->SortList();
}

/** Resort the list, e.g. when a town is renamed. */
void CargoDestinationList::Resort()
{
	this->link_list.ForceResort();
	this->SortList();
}

/**
 * Get the recommended size to display the destination list.
 * @param town Is the list displayed in the town window?
 * @return size wanted for display.
 */
Dimension CargoDestinationList::GetListSize(bool town) const
{
	uint lines = 1 + (uint)this->link_list.size();
	uint height = (lines > 10 ? 7 : 5) * FONT_HEIGHT_NORMAL; // Give long lists a bit more space.
	if (town) height *= 2;

	uint width = GetStringBoundingBox(STR_VIEW_CARGO_LAST_MONTH_OUT).width;
	for (const auto &l : this->link_list) {
		StringID str = this->PrepareDisplayString(l);
		width = max(width, GetStringBoundingBox(str).width + 1);
	}

	return Dimension(width + WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT, height + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM);
}

StringID CargoDestinationList::PrepareDisplayString(const GUICargoLink &l) const
{
	SetDParam(0, l.cid);
	SetDParam(1, l.link->amount.old_act);
	SetDParam(2, l.cid);
	SetDParam(3, l.link->amount.old_max);

	if (l.link->dest == nullptr) return STR_VIEW_CARGO_LAST_MONTH_OTHER;
	if (l.link->dest == this->obj) return STR_VIEW_CARGO_LAST_MONTH_LOCAL;

	SetDParam(4, l.link->dest->GetID());
	return l.link->dest->GetType() == ST_TOWN ? STR_VIEW_CARGO_LAST_MONTH_TOWN : STR_VIEW_CARGO_LAST_MONTH_INDUSTRY;
}
/**
 * Draw the destination list.
 * @param r The rect to draw into.
 * @param pos First visible line.
 */
void CargoDestinationList::DrawList(const Rect &r, int pos) const
{
	int y = r.top + WD_FRAMERECT_TOP;
	int lines = (r.bottom - r.top + 1 - WD_FRAMERECT_TOP - WD_FRAMERECT_BOTTOM) / FONT_HEIGHT_NORMAL;

	if (--pos < 0) {
		DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_VIEW_CARGO_LAST_MONTH_OUT);
		y += FONT_HEIGHT_NORMAL;
	}

	if (this->link_list.empty()) {
		DrawString(r.left + WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, STR_VIEW_CARGO_LAST_MONTH_NONE);
		y += FONT_HEIGHT_NORMAL;
	}

	for (const auto &l : this->link_list) {
		if (pos <= -lines) break;
		if (--pos >= 0) continue;

		/* Select string according to the destination type. */
		StringID str = this->PrepareDisplayString(l);
		DrawString(r.left + 2 * WD_FRAMERECT_LEFT, r.right - WD_FRAMERECT_RIGHT, y, str);
		y += FONT_HEIGHT_NORMAL;
	}
}

void CargoDestinationList::OnClick(int pos) const
{
	if (pos == 0 || pos - 1 >= this->link_list.size()) return;

	const CargoSourceSink *dest = this->link_list[pos - 1].link->dest;
	if (dest == nullptr) return;

	if (_ctrl_pressed) {
		ShowExtraViewPortWindow(dest->GetXY());
	} else {
		ScrollMainWindowToTile(dest->GetXY());
	}
}
