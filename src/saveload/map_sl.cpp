/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file map_sl.cpp Code handling saving and loading of map */

#include "../stdafx.h"
#include "../map_func.h"
#include "../core/bitmath_func.hpp"
#include "../fios.h"
#include <array>

#include "saveload.h"

#include "../safeguards.h"

static uint32 _map_dim_x;
static uint32 _map_dim_y;

static const SaveLoadGlobVarList _map_dimensions[] = {
	SLEG_CONDVAR(_map_dim_x, SLE_UINT32, SLV_6, SL_MAX_VERSION),
	SLEG_CONDVAR(_map_dim_y, SLE_UINT32, SLV_6, SL_MAX_VERSION),
	    SLEG_END()
};

static void Save_MAPS()
{
	_map_dim_x = MapSizeX();
	_map_dim_y = MapSizeY();
	SlGlobList(_map_dimensions);
}

static void Load_MAPS()
{
	SlGlobList(_map_dimensions);
	_m.Allocate(_map_dim_x, _map_dim_y);
}

static void Check_MAPS()
{
	SlGlobList(_map_dimensions);
	_load_check_data.map_size_x = _map_dim_x;
	_load_check_data.map_size_y = _map_dim_y;
}

static const uint MAP_SL_BUF_SIZE = 4096;

void Load_MAPR()
{
	std::array<uint, MAX_MAP_SIZE> buf;

	/* Resize each map line to the final length. */
	SlArray(buf.data(), MapSizeY(), SLE_UINT);
	for (uint i = 0; i < MapSizeY(); i++) _m.tiles[i].resize(buf[i]);

	/* Load offset table. */
	SlArray(_m.offset.data(), MapSizeX() * MapSizeY(), SLE_UINT16);
}

void Save_MAPR()
{
	SlSetLength(MapSizeY() * sizeof(uint) + MapSizeY() * MapSizeX() * sizeof(uint16));

	/* Save length of each map line. */
	std::array<uint, MAX_MAP_SIZE> buf;
	for (uint i = 0; i < MapSizeY(); i++) buf[i] = (uint)_m.tiles[i].size();
	SlArray(buf.data(), MapSizeY(), SLE_UINT);

	/* Save offset table. */
	SlArray(_m.offset.data(), MapSizeX() * MapSizeY(), SLE_UINT16);
}

static void Load_MAPT()
{
	std::array<byte, MAP_SL_BUF_SIZE> buf;
	size_t size = _m.GetTileCount();

	Map::Iterator i = _m.begin();
	while (size > 0) {
		size_t chunk = min(size, buf.size());
		SlArray(buf.data(), chunk, SLE_UINT8);
		for (uint j = 0; j < chunk; ++j, ++i) i->type = buf[j];
		size -= chunk;
	}
}

static void Save_MAPT()
{
	std::array<byte, MAP_SL_BUF_SIZE> buf;
	size_t size = _m.GetTileCount();

	SlSetLength(size);
	Map::Iterator i = _m.begin();
	while (size > 0) {
		size_t chunk = min(size, buf.size());
		for (uint j = 0; j < chunk; ++j, ++i) buf[j] = i->type;
		SlArray(buf.data(), chunk, SLE_UINT8);
		size -= chunk;
	}
}

static void Load_MAPH()
{
	std::array<byte, MAP_SL_BUF_SIZE> buf;
	size_t size = _m.GetTileCount();

	Map::Iterator i = _m.begin();
	while (size > 0) {
		size_t chunk = min(size, buf.size());
		SlArray(buf.data(), chunk, SLE_UINT8);
		for (uint j = 0; j < chunk; ++j, ++i) i->height = buf[j];
		size -= chunk;
	}
}

static void Save_MAPH()
{
	std::array<byte, MAP_SL_BUF_SIZE> buf;
	size_t size = _m.GetTileCount();

	SlSetLength(size);
	Map::Iterator i = _m.begin();
	while (size > 0) {
		size_t chunk = min(size, buf.size());
		for (uint j = 0; j < chunk; ++j, ++i) buf[j] = i->height;
		SlArray(buf.data(), chunk, SLE_UINT8);
		size -= chunk;
	}
}

static void Load_MAP1()
{
	std::array<byte, MAP_SL_BUF_SIZE> buf;
	size_t size = _m.GetTileCount();

	Map::Iterator i = _m.begin();
	while (size > 0) {
		size_t chunk = min(size, buf.size());
		SlArray(buf.data(), chunk, SLE_UINT8);
		for (uint j = 0; j < chunk; ++j, ++i) i->m1 = buf[j];
		size -= chunk;
	}
}

static void Save_MAP1()
{
	std::array<byte, MAP_SL_BUF_SIZE> buf;
	size_t size = _m.GetTileCount();

	SlSetLength(size);
	Map::Iterator i = _m.begin();
	while (size > 0) {
		size_t chunk = min(size, buf.size());
		for (uint j = 0; j < chunk; ++j, ++i) buf[j] = i->m1;
		SlArray(buf.data(), chunk, SLE_UINT8);
		size -= chunk;
	}
}

static void Load_MAP2()
{
	std::array<uint16, MAP_SL_BUF_SIZE> buf;
	size_t size = _m.GetTileCount();

	Map::Iterator i = _m.begin();
	while (size > 0) {
		size_t chunk = min(size, buf.size());
		SlArray(buf.data(), chunk,
			/* In those versions the m2 was 8 bits */
			IsSavegameVersionBefore(SLV_5) ? SLE_FILE_U8 | SLE_VAR_U16 : SLE_UINT16
		);
		for (uint j = 0; j < chunk; ++j, ++i) i->m2 = buf[j];
		size -= chunk;
	}
}

static void Save_MAP2()
{
	std::array<uint16, MAP_SL_BUF_SIZE> buf;
	size_t size = _m.GetTileCount();

	SlSetLength(size * sizeof(uint16));
	Map::Iterator i = _m.begin();
	while (size > 0) {
		size_t chunk = min(size, buf.size());
		for (uint j = 0; j < chunk; ++j, ++i) buf[j] = i->m2;
		SlArray(buf.data(), chunk, SLE_UINT16);
		size -= chunk;
	}
}

static void Load_MAP3()
{
	std::array<byte, MAP_SL_BUF_SIZE> buf;
	size_t size = _m.GetTileCount();

	Map::Iterator i = _m.begin();
	while (size > 0) {
		size_t chunk = min(size, buf.size());
		SlArray(buf.data(), chunk, SLE_UINT8);
		for (uint j = 0; j < chunk; ++j, ++i) i->m3 = buf[j];
		size -= chunk;
	}
}

static void Save_MAP3()
{
	std::array<byte, MAP_SL_BUF_SIZE> buf;
	size_t size = _m.GetTileCount();

	SlSetLength(size);
	Map::Iterator i = _m.begin();
	while (size > 0) {
		size_t chunk = min(size, buf.size());
		for (uint j = 0; j < chunk; ++j, ++i) buf[j] = i->m3;
		SlArray(buf.data(), chunk, SLE_UINT8);
		size -= chunk;
	}
}

static void Load_MAP4()
{
	std::array<byte, MAP_SL_BUF_SIZE> buf;
	size_t size = _m.GetTileCount();

	Map::Iterator i = _m.begin();
	while (size > 0) {
		size_t chunk = min(size, buf.size());
		SlArray(buf.data(), chunk, SLE_UINT8);
		for (uint j = 0; j < chunk; ++j, ++i) i->m4 = buf[j];
		size -= chunk;
	}
}

static void Save_MAP4()
{
	std::array<byte, MAP_SL_BUF_SIZE> buf;
	size_t size = _m.GetTileCount();

	SlSetLength(size);
	Map::Iterator i = _m.begin();
	while (size > 0) {
		size_t chunk = min(size, buf.size());
		for (uint j = 0; j < chunk; ++j, ++i) buf[j] = i->m4;
		SlArray(buf.data(), chunk, SLE_UINT8);
		size -= chunk;
	}
}

static void Load_MAP5()
{
	std::array<byte, MAP_SL_BUF_SIZE> buf;
	size_t size = _m.GetTileCount();

	Map::Iterator i = _m.begin();
	while (size > 0) {
		size_t chunk = min(size, buf.size());
		SlArray(buf.data(), chunk, SLE_UINT8);
		for (uint j = 0; j < chunk; ++j, ++i) i->m5 = buf[j];
		size -= chunk;
	}
}

static void Save_MAP5()
{
	std::array<byte, MAP_SL_BUF_SIZE> buf;
	size_t size = _m.GetTileCount();

	SlSetLength(size);
	Map::Iterator i = _m.begin();
	while (size > 0) {
		size_t chunk = min(size, buf.size());
		for (uint j = 0; j < chunk; ++j, ++i) buf[j] = i->m5;
		SlArray(buf.data(), chunk, SLE_UINT8);
		size -= chunk;
	}
}

static void Load_MAP6()
{
	std::array<byte, MAP_SL_BUF_SIZE> buf;
	size_t size = _m.GetTileCount();

	if (IsSavegameVersionBefore(SLV_42)) {
		Map::Iterator i = _m.begin();
		while (size > 0) {
			size_t chunk = min(size, buf.size());
			/* There are four tiles packed into one byte! */
			SlArray(buf.data(), chunk / 4, SLE_UINT8);
			for (uint j = 0; j < chunk / 4; ++j) {
				(i++)->m6 = GB(buf[j], 0, 2);
				(i++)->m6 = GB(buf[j], 2, 2);
				(i++)->m6 = GB(buf[j], 4, 2);
				(i++)->m6 = GB(buf[j], 6, 2);
			}
			size -= chunk;
		}
	} else {
		Map::Iterator i = _m.begin();
		while (size > 0) {
			size_t chunk = min(size, buf.size());
			SlArray(buf.data(), chunk, SLE_UINT8);
			for (uint j = 0; j < chunk; ++j, ++i) i->m6 = buf[j];
			size -= chunk;
		}
	}
}

static void Save_MAP6()
{
	std::array<byte, MAP_SL_BUF_SIZE> buf;
	size_t size = _m.GetTileCount();

	SlSetLength(size);
	Map::Iterator i = _m.begin();
	while (size > 0) {
		size_t chunk = min(size, buf.size());
		for (uint j = 0; j < chunk; ++j, ++i) buf[j] = i->m6;
		SlArray(buf.data(), chunk, SLE_UINT8);
		size -= chunk;
	}
}

static void Load_MAP7()
{
	std::array<byte, MAP_SL_BUF_SIZE> buf;
	size_t size = _m.GetTileCount();

	Map::Iterator i = _m.begin();
	while (size > 0) {
		size_t chunk = min(size, buf.size());
		SlArray(buf.data(), chunk, SLE_UINT8);
		for (uint j = 0; j < chunk; ++j, ++i) i->m7 = buf[j];
		size -= chunk;
	}
}

static void Save_MAP7()
{
	std::array<byte, MAP_SL_BUF_SIZE> buf;
	size_t size = _m.GetTileCount();

	SlSetLength(size);
	Map::Iterator i = _m.begin();
	while (size > 0) {
		size_t chunk = min(size, buf.size());
		for (uint j = 0; j < chunk; ++j, ++i) buf[j] = i->m7;
		SlArray(buf.data(), chunk, SLE_UINT8);
		size -= chunk;
	}
}

static void Load_MAP8()
{
	std::array<uint16, MAP_SL_BUF_SIZE> buf;
	size_t size = _m.GetTileCount();

	Map::Iterator i = _m.begin();
	while (size > 0) {
		size_t chunk = min(size, buf.size());
		SlArray(buf.data(), chunk, SLE_UINT16);
		for (uint j = 0; j < chunk; ++j, ++i) i->m8 = buf[j];
		size -= chunk;
	}
}

static void Save_MAP8()
{
	std::array<uint16, MAP_SL_BUF_SIZE> buf;
	size_t size = _m.GetTileCount();

	SlSetLength(size * sizeof(uint16));
	Map::Iterator i = _m.begin();
	while (size > 0) {
		size_t chunk = min(size, buf.size());
		for (uint j = 0; j < chunk; ++j, ++i) buf[j] = i->m8;
		SlArray(buf.data(), chunk, SLE_UINT16);
		size -= chunk;
	}
}


extern const ChunkHandler _map_chunk_handlers[] = {
	{ 'MAPS', Save_MAPS, Load_MAPS, nullptr, Check_MAPS, CH_RIFF },
	{ 'MAPR', Save_MAPR, Load_MAPR, nullptr, nullptr,    CH_RIFF },
	{ 'MAPT', Save_MAPT, Load_MAPT, nullptr, nullptr,    CH_RIFF },
	{ 'MAPH', Save_MAPH, Load_MAPH, nullptr, nullptr,    CH_RIFF },
	{ 'MAPO', Save_MAP1, Load_MAP1, nullptr, nullptr,    CH_RIFF },
	{ 'MAP2', Save_MAP2, Load_MAP2, nullptr, nullptr,    CH_RIFF },
	{ 'M3LO', Save_MAP3, Load_MAP3, nullptr, nullptr,    CH_RIFF },
	{ 'M3HI', Save_MAP4, Load_MAP4, nullptr, nullptr,    CH_RIFF },
	{ 'MAP5', Save_MAP5, Load_MAP5, nullptr, nullptr,    CH_RIFF },
	{ 'MAPE', Save_MAP6, Load_MAP6, nullptr, nullptr,    CH_RIFF },
	{ 'MAP7', Save_MAP7, Load_MAP7, nullptr, nullptr,    CH_RIFF },
	{ 'MAP8', Save_MAP8, Load_MAP8, nullptr, nullptr,    CH_RIFF | CH_LAST },
};
