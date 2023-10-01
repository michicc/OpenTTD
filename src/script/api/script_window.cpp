/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_window.cpp Implementation of ScriptWindow. */

#include "../../stdafx.h"
#include "script_window.hpp"
#include "script_game.hpp"
#include "../../window_func.h"
#include "../../window_gui.h"
#include "../../consist_base.h"

#include "../../safeguards.h"

static WindowNumber TranslateVehicleWindowNumber(ScriptWindow::WindowClass window, WindowNumber number)
{
	if (number == ScriptWindow::NUMBER_ALL) return number;

	switch (window) {
		case ScriptWindow::WC_VEHICLE_ORDERS:
		case ScriptWindow::WC_VEHICLE_TIMETABLE:
			return ::Vehicle::IsValidID(number) && ::Vehicle::Get(number)->GetConsist() != nullptr ? ::Vehicle::Get(number)->GetConsist()->index : ScriptWindow::NUMBER_ALL;
		default:
			return number;
	}
}

/* static */ void ScriptWindow::Close(WindowClass window, SQInteger number)
{
	if (ScriptGame::IsMultiplayer()) return;

	if (number == NUMBER_ALL) {
		CloseWindowByClass((::WindowClass)window);
		return;
	}

	number = Clamp<SQInteger>(number, 0, INT32_MAX);

	CloseWindowById((::WindowClass)window, TranslateVehicleWindowNumber(window, number));
}

/* static */ bool ScriptWindow::IsOpen(WindowClass window, SQInteger number)
{
	if (ScriptGame::IsMultiplayer()) return false;

	if (number == NUMBER_ALL) {
		return (FindWindowByClass((::WindowClass)window) != nullptr);
	}

	number = Clamp<SQInteger>(number, 0, INT32_MAX);

	return FindWindowById((::WindowClass)window, TranslateVehicleWindowNumber(window, number)) != nullptr;
}

/* static */ void ScriptWindow::Highlight(WindowClass window, SQInteger number, SQInteger widget, TextColour colour)
{
	if (ScriptGame::IsMultiplayer()) return;
	if (number == NUMBER_ALL) return;
	if (!IsOpen(window, number)) return;
	if (colour != TC_INVALID && (::TextColour)colour >= ::TC_END) return;

	number = Clamp<SQInteger>(number, 0, INT32_MAX);

	Window *w = FindWindowById((::WindowClass)window, TranslateVehicleWindowNumber(window, number));
	assert(w != nullptr);

	if (widget == WIDGET_ALL) {
		if (colour != TC_INVALID) return;
		w->DisableAllWidgetHighlight();
		return;
	}

	widget = Clamp<SQInteger>(widget, 0, UINT8_MAX);

	const NWidgetBase *wid = w->GetWidget<NWidgetBase>(widget);
	if (wid == nullptr) return;
	w->SetWidgetHighlight(widget, (::TextColour)colour);
}
