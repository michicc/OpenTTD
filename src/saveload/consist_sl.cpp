/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file consist_sl.cpp Code handling saving and loading of vehicle consists. */

#include "../stdafx.h"
#include "../consist_base.h"
#include "../aircraft.h"
#include "../roadveh.h"
#include "../ship.h"
#include "../train.h"
#include "saveload.h"

#include "../safeguards.h"

struct CNSTChunkHandler : ChunkHandler {
	CNSTChunkHandler() : ChunkHandler('CNST', CH_TABLE) {}

	inline static const SaveLoad _consist_desc[] = {
		 SLE_SAVEBYTE(Consist, type),
		 SLE_VAR(Consist, owner,                     SLE_UINT8),
		SLE_SSTR(Consist, name,                      SLE_STR | SLF_ALLOW_CONTROL),
		 SLE_VAR(Consist, current_order_time,        SLE_UINT32),
		 SLE_VAR(Consist, lateness_counter,          SLE_INT32),
		 SLE_VAR(Consist, timetable_start,           SLE_UINT64),
		 SLE_VAR(Consist, service_interval,          SLE_UINT16),
		 SLE_VAR(Consist, cur_real_order_index,      SLE_UINT8),
		 SLE_VAR(Consist, cur_implicit_order_index,  SLE_UINT8),
		 SLE_VAR(Consist, consist_flags,             SLE_UINT16),
		 SLE_REF(Consist, front,                     REF_VEHICLE),
		 SLE_VAR(Consist, last_loading_tick,         SLE_UINT64),
	};

	void Save() const override
	{
		SlTableHeader(_consist_desc);

		for (Consist *cs : Consist::Iterate()) {
			SlSetArrayIndex(cs->index);
			SlObject(cs, _consist_desc);
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlTableHeader(_consist_desc);

		int index;
		while ((index = SlIterateArray()) != -1) {
			VehicleType vtype = (VehicleType)SlReadByte();

			Consist *cs;
			switch (vtype) {
				case VEH_TRAIN:    cs = new (index) TrainConsist();    break;
				case VEH_ROAD:     cs = new (index) RoadConsist();     break;
				case VEH_SHIP:     cs = new (index) ShipConsist();     break;
				case VEH_AIRCRAFT: cs = new (index) AircraftConsist(); break;
				default: SlErrorCorrupt("Invalid consist type");
			}

			SlObject(cs, slt);
		}
	}

	void FixPointers() const override
	{
		if (IsSavegameVersionBefore(SLV_CONSISTS)) return;

		for (Consist *cs : Consist::Iterate()) {
			SlObject(cs, _consist_desc);
		}
	}
};

static const CNSTChunkHandler CNST;
static const ChunkHandlerRef consist_chunk_handlers[] = {
	CNST,
};

extern const ChunkHandlerTable _consist_chunk_handlers(consist_chunk_handlers);
