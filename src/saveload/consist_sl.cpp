/* $Id$ */

/*
* This file is part of OpenTTD.
* OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
* OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
*/

/** @file consist_sl.cpp Code handling saving and loading of consists. */

#include "../stdafx.h"
#include "../consist_base.h"

#include "saveload.h"

#include "../safeguards.h"


const struct SaveLoad *GetConsistDescription()
{
	static const SaveLoad _consist_desc[] = {
		     SLE_VAR(Consist, type,                      SLE_UINT8),
		     SLE_VAR(Consist, name,                      SLE_STR | SLF_ALLOW_CONTROL),
		     SLE_VAR(Consist, current_order_time,        SLE_UINT32),
		     SLE_VAR(Consist, lateness_counter,          SLE_INT32),
		     SLE_VAR(Consist, timetable_start,           SLE_UINT32),
		     SLE_VAR(Consist, service_interval,          SLE_UINT16),
		     SLE_VAR(Consist, cur_real_order_index,      SLE_UINT8),
		     SLE_VAR(Consist, cur_implicit_order_index,  SLE_UINT8),
		     SLE_VAR(Consist, consist_flags,             SLE_UINT16),
		     SLE_REF(Consist, front,                     REF_CONSIST),
		     SLE_VAR(Consist, owner,                     SLE_UINT8),
		     SLE_END()
	};

	return _consist_desc;
}

/** Will be called when the consists need to be saved. */
static void Save_CNST()
{
	if (IsSavegameVersionBefore(196)) return;

	Consist *cs;
	/* Write the consists. */
	FOR_ALL_CONSISTS(cs) {
		SlSetArrayIndex(cs->index);
		SlObject(cs, GetConsistDescription());
	}
}

/** Will be called when consists need to be loaded. */
void Load_CNST()
{
	int index;

	while ((index = SlIterateArray()) != -1) {
		Consist *cs = new (index) Consist();

		SlObject(cs, GetConsistDescription());
	}
}

static void Ptrs_CNST()
{
	if (IsSavegameVersionBefore(196)) return;

	Consist *cs;
	FOR_ALL_CONSISTS(cs) {
		SlObject(cs, GetConsistDescription());
	}
}

extern const ChunkHandler _consist_chunk_handlers[] = {
	{ 'CNST', Save_CNST, Load_CNST, Ptrs_CNST, NULL, CH_SPARSE_ARRAY | CH_LAST },
};
