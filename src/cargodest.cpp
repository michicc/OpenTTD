/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cargodest.cpp Implementation of cargo destinations. */

#include "stdafx.h"
#include "cargodest_base.h"
#include "industry.h"
#include "town.h"
#include <algorithm>

#include "safeguards.h"


/* virtual */ CargoSourceSink::~CargoSourceSink()
{
	if (Town::CleaningPool() || Industry::CleaningPool()) return;

	/* Remove all demand links having us as a destination. */
	for (Town *t : Town::Iterate()) {
		for (CargoID cid = 0; cid < NUM_CARGO; cid++) {
			t->cargo_links[cid].erase(std::remove(t->cargo_links[cid].begin(), t->cargo_links[cid].end(), this), t->cargo_links[cid].end());
		}
	}
	for (Industry *ind : Industry::Iterate()) {
		for (CargoID cid = 0; cid < NUM_CARGO; cid++) {
			ind->cargo_links[cid].erase(std::remove(ind->cargo_links[cid].begin(), ind->cargo_links[cid].end(), this), ind->cargo_links[cid].end());
		}
	}
}

void CargoSourceSink::UpdateLinkWeightSums()
{
	for (CargoID cid = 0; cid < NUM_CARGO; cid++) {
		uint weight_sum = 0;
		for (const auto &l : this->cargo_links[cid]) {
			weight_sum += l.weight;
		}

		this->cargo_links_weight[cid] = weight_sum;
	}
}
