/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file cargodest_gui.cpp GUI for cargo destinations. */

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

bool CargoLinkSorter(const CargoSourceSink *cur_css, const GUICargoDemandLink &a, const GUICargoDemandLink &b)
{
	/* Sort by cargo type. */
	if (a.cargo_type != b.cargo_type) return a.cargo_type < b.cargo_type;

	/* Sort unspecified destination links always last. */
	if (a.link->dest == nullptr) return false;
	if (b.link->dest == nullptr) return true;

	/* Sort link with the current source as destination first. */
	if (a.link->dest == cur_css) return true;
	if (b.link->dest == cur_css) return false;

	/* Sort towns before industries. */
	if (a.link->dest->ToSource().type != b.link->dest->ToSource().type) {
		return a.link->dest->ToSource().type < b.link->dest->ToSource().type;
	}

	/* Sort by name. */
	auto name = GetString(a.link->dest->ToSource().GetFormat(), a.link->dest->ToSource().id);

	/* Cache name lookup of 'b', as the sorter is often called multiple times with the same 'b'. */
	static const CargoDemandLink *last_b = nullptr;
	static std::string last_name{};
	if (b.link != last_b) {
		last_b = b.link;
		last_name = GetString(b.link->dest->ToSource().GetFormat(), b.link->dest->ToSource().id);
	}

	return StrNaturalCompare(name, last_name) < 0;
}

CargoDestinationList::CargoDestinationList(const CargoSourceSink *css) : obj(css)
{
	this->InvalidateData();
}

/** Rebuild the link list from the source object. */
void CargoDestinationList::RebuildList()
{
	if (!this->link_list.NeedRebuild()) return;

	this->link_list.clear();
	for (CargoType cargo_type = 0; cargo_type < this->obj->cargo_links.size(); ++cargo_type) {
		for (const CargoDemandLink &l : this->obj->cargo_links[cargo_type]) {
			this->link_list.emplace_back(cargo_type, &l);
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
 * Get display string for a demand link.
 * @param link Demand link.
 * @return Formatted string.
 */
std::string CargoDestinationList::GetDisplayString(const GUICargoDemandLink &link) const
{
	if (link.link->dest == nullptr) GetString(STR_VIEW_CARGO_LAST_MONTH_OTHER, link.cargo_type, link.link->amount.old_act, link.cargo_type, link.link->amount.old_max);
	if (link.link->dest == this->obj) GetString(STR_VIEW_CARGO_LAST_MONTH_OTHER, link.cargo_type, link.link->amount.old_act, link.cargo_type, link.link->amount.old_max);

	auto dest = link.link->dest->ToSource();
	return GetString(STR_VIEW_CARGO_LAST_MONTH_DEST, link.cargo_type, link.link->amount.old_act, link.cargo_type, link.link->amount.old_max, dest.GetFormat(), dest.id);
}

/**
 * Get the recommended size to display the destination list.
 * @param town Is the list displayed in the town window?
 * @return Size wanted for display.
 */
Dimension CargoDestinationList::GetListSize(bool town) const
{
	size_t lines = this->GetLineCount();

	uint height = (lines > 10 ? 7 : 5) * GetCharacterHeight(FS_NORMAL); // Give long lists a bit more space.
	if (town) height *= 2;

	uint width = GetStringBoundingBox(STR_VIEW_CARGO_LAST_MONTH_OUT).width;
	for (const auto &link : this->link_list) {
		width = std::max(width, GetStringBoundingBox(this->GetDisplayString(link)).width + 1);
	}

	return Dimension(width + WidgetDimensions::scaled.framerect.Horizontal(), height + WidgetDimensions::scaled.framerect.Vertical());
}

/**
 * Draw the destination list.
 * @param r The rect to draw into.
 * @param pos First visible line.
 */
void CargoDestinationList::DrawList(const Rect &r, int pos) const
{
	Rect r2 = r.Shrink(WidgetDimensions::scaled.framerect);
	int char_height = GetCharacterHeight(FS_NORMAL);

	int y = r2.top;
	int lines = (r2.bottom - y + 1) / char_height;

	if (--pos < 0) {
		DrawString(r2.left, r2.right, y, STR_VIEW_CARGO_LAST_MONTH_OUT);
		y += char_height;
	}

	if (this->link_list.empty()) {
		DrawString(r2.left, r2.right, y, STR_VIEW_CARGO_LAST_MONTH_NONE);
	} else {
		for (const auto &link : this->link_list) {
			if (pos <= -lines) break;
			if (--pos >= 0) continue;

			DrawString(r2.left, r2.right, y, this->GetDisplayString(link));
			y += char_height;
		}
	}
}

/**
 * Handle clicking a destination list line.
 * @param pos Line that was clicked.
 */
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
