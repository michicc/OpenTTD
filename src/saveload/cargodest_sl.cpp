/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cargodest_sl.cpp Code handling saving and loading of cargo destinations. */

#include "../stdafx.h"
#include "../cargodest_base.h"
#include "../town.h"
#include "../industry.h"
#include "saveload.h"

static uint32 _cargolink_uint;
static const SaveLoadGlobVarList _cargolink_uint_desc[] = {
	SLEG_VAR(_cargolink_uint, SLE_UINT32),
	SLEG_END()
};

static const SaveLoad _cargolink_desc[] = {
	SLE_VAR(CargoLink, amount.old_max, SLE_UINT32),
	SLE_VAR(CargoLink, amount.new_max, SLE_UINT32),
	SLE_VAR(CargoLink, amount.old_act, SLE_UINT32),
	SLE_VAR(CargoLink, amount.new_act, SLE_UINT32),
	SLE_VAR(CargoLink, weight,         SLE_UINT32),
	SLE_VAR(CargoLink, weight_mod,     SLE_UINT8),
	SLE_END()
};


void CargoSourceSink::SaveCargoSourceSink()
{
	for (uint cid = 0; cid < lengthof(this->cargo_links); cid++) {
		_cargolink_uint = (uint32)this->cargo_links[cid].size();
		SlGlobList(_cargolink_uint_desc);

		for (auto &l : this->cargo_links[cid]) {
			SourceID dest = INVALID_SOURCE;
			SourceType type = ST_TOWN;

			if (l.dest != nullptr) {
				type = l.dest->GetType();
				dest = l.dest->GetID();
			}

			/* Pack type and destination index into temp variable. */
			assert_compile(sizeof(SourceID) <= 3);
			_cargolink_uint = type | (dest << 8);

			SlGlobList(_cargolink_uint_desc);
			SlObject(&l, _cargolink_desc);
		}
	}
}

void CargoSourceSink::LoadCargoSourceSink()
{
	if (IsSavegameVersionBefore(SLV_YACD)) return;

	for (uint cid = 0; cid < lengthof(this->cargo_links); cid++) {
		/* Read vector length and allocate storage. */
		SlObject(nullptr, _cargolink_uint_desc);
		this->cargo_links[cid].clear();
		this->cargo_links[cid].resize(_cargolink_uint);

		for (auto &l : this->cargo_links[cid]) {
			/* Read packed type and dest and store in dest pointer. */
			SlGlobList(_cargolink_uint_desc);
			l.dest = reinterpret_cast<CargoSourceSink *>((size_t)_cargolink_uint);

			SlObject(&l, _cargolink_desc);
		}
	}

	this->UpdateLinkWeightSums();
}

void CargoSourceSink::PtrsCargoSourceSink()
{
	if (IsSavegameVersionBefore(SLV_YACD)) return;

	for (uint cid = 0; cid < lengthof(this->cargo_links); cid++) {
		for (auto &l : this->cargo_links[cid]) {
			/* Extract type and destination index. */
			SourceType type = (SourceType)(reinterpret_cast<size_t>(l.dest) & 0xFF);
			SourceID dest = (SourceID)(reinterpret_cast<size_t>(l.dest) >> 8);

			/* Resolve index. */
			if (dest != INVALID_SOURCE) {
				switch (type) {
					case ST_INDUSTRY:
						if (!Industry::IsValidID(dest)) SlErrorCorrupt("Invalid cargo link destination");
						l.dest = Industry::Get(dest);
						break;

					case ST_TOWN:
						if (!Town::IsValidID(dest)) SlErrorCorrupt("Invalid cargo link destination");
						l.dest = Town::Get(dest);
						break;

					default:
						SlErrorCorrupt("Invalid cargo link destination type");
				}
			} else {
				l.dest = nullptr;
			}
		}
	}
}
