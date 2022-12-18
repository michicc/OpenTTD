/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cargodest.cpp GUI for cargo destinations. */

#include "stdafx.h"
#include "cargodest_gui.h"
#include "gfx_func.h"
#include "gui.h"
#include "string_func.h"
#include "strings_func.h"
#include "viewport_func.h"
#include "window_gui.h"

#include "table/strings.h"

#include "safeguards.h"

bool CargoLinkSorter(const CargoSourceSink *cur_css, const GUICargoLink &a, const GUICargoLink &b)
{
	/* Sort by cargo type. */
	if (a.cid != b.cid) return a.cid < b.cid;

	/* Sort unspecified destination links always last. */
	if (a.link->dest == nullptr) return false;
	if (b.link->dest == nullptr) return true;

	/* Sort link with the current source as destination first. */
	if (a.link->dest == cur_css) return true;
	if (b.link->dest == cur_css) return false;

	/* Sort towns before industries. */
	if (a.link->dest->GetType() != b.link->dest->GetType()) {
		return a.link->dest->GetType() < b.link->dest->GetType();
	}

	/* Sort by name. */
	SetDParam(0, a.link->dest->GetID());
	auto name = GetString(a.link->dest->GetType() == ST_TOWN ? STR_TOWN_NAME : STR_INDUSTRY_NAME);

	/* Cache name lookup of 'b', as the sorter is often called multiple times with the same 'b'. */
	static const CargoLink *last_b = nullptr;
	static std::string last_name{};
	if (b.link != last_b) {
		last_b = b.link;
		SetDParam(0, b.link->dest->GetID());
		last_name = GetString(b.link->dest->GetType() == ST_TOWN ? STR_TOWN_NAME : STR_INDUSTRY_NAME);
	}

	return strnatcmp(name.c_str(), last_name.c_str()) < 0;
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
	this->link_list.Sort([css = this->obj](auto a, auto b) { return CargoLinkSorter(css, a, b); });
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
 * @return Size wanted for display.
 */
Dimension CargoDestinationList::GetListSize(bool town) const
{
	uint lines = 1 + (uint)this->link_list.size();
	uint height = (lines > 10 ? 7 : 5) * FONT_HEIGHT_NORMAL; // Give long lists a bit more space.
	if (town) height *= 2;

	uint width = GetStringBoundingBox(STR_VIEW_CARGO_LAST_MONTH_OUT).width;
	for (const auto &l : this->link_list) {
		StringID str = this->PrepareDisplayString(l);
		width = std::max(width, GetStringBoundingBox(str).width + 1);
	}

	return Dimension(width + WidgetDimensions::scaled.framerect.Horizontal(), height + WidgetDimensions::scaled.framerect.Vertical());
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
	Rect r2 = r.Shrink(WidgetDimensions::scaled.framerect);

	int y = r2.top;
	int lines = (r2.bottom - y + 1) / FONT_HEIGHT_NORMAL;

	if (--pos < 0) {
		DrawString(r2.left, r2.right, y, STR_VIEW_CARGO_LAST_MONTH_OUT);
		y += FONT_HEIGHT_NORMAL;
	}

	if (this->link_list.empty()) {
		DrawString(r2.left, r2.right, y, STR_VIEW_CARGO_LAST_MONTH_NONE);
		y += FONT_HEIGHT_NORMAL;
	}

	for (const auto &l : this->link_list) {
		if (pos <= -lines) break;
		if (--pos >= 0) continue;

		/* Select string according to the destination type. */
		StringID str = this->PrepareDisplayString(l);
		DrawString(r2.left, r2.right, y, str);
		y += FONT_HEIGHT_NORMAL;
	}
}

void CargoDestinationList::OnClick(int pos) const
{
	if (pos == 0 || pos - 1 >= this->link_list.size()) return;

	const CargoSourceSink *dest = this->link_list[pos - 1].link->dest;
	if (dest == nullptr) return;

	if (_ctrl_pressed) {
		ShowExtraViewportWindow(dest->GetXY());
	} else {
		ScrollMainWindowToTile(dest->GetXY());
	}
}
