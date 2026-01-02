/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file cargodest.cpp Implementation of cargo destinations. */

#include "stdafx.h"
#include "cargodest_base.h"
#include "town.h"
#include "industry.h"
#include "window_func.h"

#include "safeguards.h"

/* virtual */ CargoSourceSink::~CargoSourceSink()
{
	if (Town::CleaningPool() || Industry::CleaningPool()) return;

	/* Remove all demand links having us as a destination. */
	for (Town *t : Town::Iterate()) {
		for (CargoType cargo = 0; cargo < NUM_CARGO; ++cargo) {
			if (std::erase(t->cargo_links[cargo], this) > 0) InvalidateWindowData(WC_TOWN_VIEW, t->index, -1);
		}
	}
	for (Industry *ind : Industry::Iterate()) {
		for (CargoType cargo = 0; cargo < NUM_CARGO; ++cargo) {
			if (std::erase(ind->cargo_links[cargo], this) > 0) InvalidateWindowData(WC_INDUSTRY_VIEW, ind->index, -1);
		}
	}
}

void CargoSourceSink::UpdateLinkWeightSums()
{
	for (CargoType cargo = 0; cargo < NUM_CARGO; ++cargo) {
		uint weight_sum = 0;
		for (const auto &l : this->cargo_links[cargo]) {
			weight_sum += l.weight;
		}
	}
}
