/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file map_sl.cpp Code handling saving and loading of map */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/map_sl_compat.h"

#include "../map_func.h"
#include "../core/bitmath_func.hpp"
#include "../fios.h"
#include <array>

#include "../safeguards.h"

static uint32 _map_dim_x;
static uint32 _map_dim_y;

static const SaveLoad _map_desc[] = {
	SLEG_CONDVAR("dim_x", _map_dim_x, SLE_UINT32, SLV_6, SL_MAX_VERSION),
	SLEG_CONDVAR("dim_y", _map_dim_y, SLE_UINT32, SLV_6, SL_MAX_VERSION),
};

struct MAPSChunkHandler : ChunkHandler {
	MAPSChunkHandler() : ChunkHandler('MAPS', CH_TABLE) {}

	void Save() const override
	{
		SlTableHeader(_map_desc);

		_map_dim_x = MapSizeX();
		_map_dim_y = MapSizeY();

		SlSetArrayIndex(0);
		SlGlobList(_map_desc);
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_map_desc, _map_sl_compat);

		if (!IsSavegameVersionBefore(SLV_RIFF_TO_ARRAY) && SlIterateArray() == -1) return;
		SlGlobList(slt);
		if (!IsSavegameVersionBefore(SLV_RIFF_TO_ARRAY) && SlIterateArray() != -1) SlErrorCorrupt("Too many MAPS entries");

		_m.Allocate(_map_dim_x, _map_dim_y);
	}

	void LoadCheck(size_t) const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_map_desc, _map_sl_compat);

		if (!IsSavegameVersionBefore(SLV_RIFF_TO_ARRAY) && SlIterateArray() == -1) return;
		SlGlobList(slt);
		if (!IsSavegameVersionBefore(SLV_RIFF_TO_ARRAY) && SlIterateArray() != -1) SlErrorCorrupt("Too many MAPS entries");

		_load_check_data.map_size_x = _map_dim_x;
		_load_check_data.map_size_y = _map_dim_y;
	}
};

static const uint MAP_SL_BUF_SIZE = 4096;

struct MAPRChunkHandler : ChunkHandler {
	MAPRChunkHandler() : ChunkHandler('MAPR', CH_RIFF) {}

	void Load() const override
	{
		std::array<uint, MAX_MAP_SIZE> buf;

		/* Resize each map line to the final length. */
		SlCopy(buf.data(), MapSizeY(), SLE_UINT);
		for (uint i = 0; i < MapSizeY(); i++) _m.tiles[i].resize(buf[i]);

		/* Load offset table. */
		SlCopy(_m.offset.data(), MapSizeX() * MapSizeY(), SLE_UINT16);
	}

	void Save() const override
	{
		SlSetLength(MapSizeY() * sizeof(uint) + MapSizeY() * MapSizeX() * sizeof(uint16));

		/* Save length of each map line. */
		std::array<uint, MAX_MAP_SIZE> buf;
		for (uint i = 0; i < MapSizeY(); i++) buf[i] = (uint)_m.tiles[i].size();
		SlCopy(buf.data(), MapSizeY(), SLE_UINT);

		/* Save offset table. */
		SlCopy(_m.offset.data(), MapSizeX() * MapSizeY(), SLE_UINT16);
	}
};

struct MAPTChunkHandler : ChunkHandler {
	MAPTChunkHandler() : ChunkHandler('MAPT', CH_RIFF) {}

	void Load() const override
	{
		std::array<byte, MAP_SL_BUF_SIZE> buf;
		size_t size = _m.GetTileCount();

		Map::Iterator i = Map::Iterate().begin();
		while (size > 0) {
			size_t chunk = std::min(size, buf.size());
			SlCopy(buf.data(), chunk, SLE_UINT8);
			for (uint j = 0; j < chunk; ++j, ++i) i->type = buf[j];
			size -= chunk;
		}
	}

	void Save() const override
	{
		std::array<byte, MAP_SL_BUF_SIZE> buf;
		size_t size = _m.GetTileCount();

		SlSetLength(size);
		Map::Iterator i = Map::Iterate().begin();
		while (size > 0) {
			size_t chunk = std::min(size, buf.size());
			for (uint j = 0; j < chunk; ++j, ++i) buf[j] = i->type;
			SlCopy(buf.data(), chunk, SLE_UINT8);
			size -= chunk;
		}
	}
};

struct MAPHChunkHandler : ChunkHandler {
	MAPHChunkHandler() : ChunkHandler('MAPH', CH_RIFF) {}

	void Load() const override
	{
		std::array<byte, MAP_SL_BUF_SIZE> buf;
		size_t size = _m.GetTileCount();

		Map::Iterator i = Map::Iterate().begin();
		while (size > 0) {
			size_t chunk = std::min(size, buf.size());
			SlCopy(buf.data(), chunk, SLE_UINT8);
			for (uint j = 0; j < chunk; ++j, ++i) i->height = buf[j];
			size -= chunk;
		}
	}

	void Save() const override
	{
		std::array<byte, MAP_SL_BUF_SIZE> buf;
		size_t size = _m.GetTileCount();

		SlSetLength(size);
		Map::Iterator i = Map::Iterate().begin();
		while (size > 0) {
			size_t chunk = std::min(size, buf.size());
			for (uint j = 0; j < chunk; ++j, ++i) buf[j] = i->height;
			SlCopy(buf.data(), chunk, SLE_UINT8);
			size -= chunk;
		}
	}
};

struct MAPOChunkHandler : ChunkHandler {
	MAPOChunkHandler() : ChunkHandler('MAPO', CH_RIFF) {}

	void Load() const override
	{
		std::array<byte, MAP_SL_BUF_SIZE> buf;
		size_t size = _m.GetTileCount();

		Map::Iterator i = Map::Iterate().begin();
		while (size > 0) {
			size_t chunk = std::min(size, buf.size());
			SlCopy(buf.data(), chunk, SLE_UINT8);
			for (uint j = 0; j < chunk; ++j, ++i) i->m1 = buf[j];
			size -= chunk;
		}
	}

	void Save() const override
	{
		std::array<byte, MAP_SL_BUF_SIZE> buf;
		size_t size = _m.GetTileCount();

		SlSetLength(size);
		Map::Iterator i = Map::Iterate().begin();
		while (size > 0) {
			size_t chunk = std::min(size, buf.size());
			for (uint j = 0; j < chunk; ++j, ++i) buf[j] = i->m1;
			SlCopy(buf.data(), chunk, SLE_UINT8);
			size -= chunk;
		}
	}
};

struct MAP2ChunkHandler : ChunkHandler {
	MAP2ChunkHandler() : ChunkHandler('MAP2', CH_RIFF) {}

	void Load() const override
	{
		std::array<uint16, MAP_SL_BUF_SIZE> buf;
		size_t size = _m.GetTileCount();

		Map::Iterator i = Map::Iterate().begin();
		while (size > 0) {
			size_t chunk = std::min(size, buf.size());
			SlCopy(buf.data(), chunk,
				/* In those versions the m2 was 8 bits */
				IsSavegameVersionBefore(SLV_5) ? SLE_FILE_U8 | SLE_VAR_U16 : SLE_UINT16
			);
			for (uint j = 0; j < chunk; ++j, ++i) i->m2 = buf[j];
			size -= chunk;
		}
	}

	void Save() const override
	{
		std::array<uint16, MAP_SL_BUF_SIZE> buf;
		size_t size = _m.GetTileCount();

		SlSetLength(size * sizeof(uint16));
		Map::Iterator i = Map::Iterate().begin();
		while (size > 0) {
			size_t chunk = std::min(size, buf.size());
			for (uint j = 0; j < chunk; ++j, ++i) buf[j] = i->m2;
			SlCopy(buf.data(), chunk, SLE_UINT16);
			size -= chunk;
		}
	}
};

struct M3LOChunkHandler : ChunkHandler {
	M3LOChunkHandler() : ChunkHandler('M3LO', CH_RIFF) {}

	void Load() const override
	{
		std::array<byte, MAP_SL_BUF_SIZE> buf;
		size_t size = _m.GetTileCount();

		Map::Iterator i = Map::Iterate().begin();
		while (size > 0) {
			size_t chunk = std::min(size, buf.size());
			SlCopy(buf.data(), chunk, SLE_UINT8);
			for (uint j = 0; j < chunk; ++j, ++i) i->m3 = buf[j];
			size -= chunk;
		}
	}

	void Save() const override
	{
		std::array<byte, MAP_SL_BUF_SIZE> buf;
		size_t size = _m.GetTileCount();

		SlSetLength(size);
		Map::Iterator i = Map::Iterate().begin();
		while (size > 0) {
			size_t chunk = std::min(size, buf.size());
			for (uint j = 0; j < chunk; ++j, ++i) buf[j] = i->m3;
			SlCopy(buf.data(), chunk, SLE_UINT8);
			size -= chunk;
		}
	}
};

struct M3HIChunkHandler : ChunkHandler {
	M3HIChunkHandler() : ChunkHandler('M3HI', CH_RIFF) {}

	void Load() const override
	{
		std::array<byte, MAP_SL_BUF_SIZE> buf;
		size_t size = _m.GetTileCount();

		Map::Iterator i = Map::Iterate().begin();
		while (size > 0) {
			size_t chunk = std::min(size, buf.size());
			SlCopy(buf.data(), chunk, SLE_UINT8);
			for (uint j = 0; j < chunk; ++j, ++i) i->m4 = buf[j];
			size -= chunk;
		}
	}

	void Save() const override
	{
		std::array<byte, MAP_SL_BUF_SIZE> buf;
		size_t size = _m.GetTileCount();

		SlSetLength(size);
		Map::Iterator i = Map::Iterate().begin();
		while (size > 0) {
			size_t chunk = std::min(size, buf.size());
			for (uint j = 0; j < chunk; ++j, ++i) buf[j] = i->m4;
			SlCopy(buf.data(), chunk, SLE_UINT8);
			size -= chunk;
		}
	}
};

struct MAP5ChunkHandler : ChunkHandler {
	MAP5ChunkHandler() : ChunkHandler('MAP5', CH_RIFF) {}

	void Load() const override
	{
		std::array<byte, MAP_SL_BUF_SIZE> buf;
		size_t size = _m.GetTileCount();

		Map::Iterator i = Map::Iterate().begin();
		while (size > 0) {
			size_t chunk = std::min(size, buf.size());
			SlCopy(buf.data(), chunk, SLE_UINT8);
			for (uint j = 0; j < chunk; ++j, ++i) i->m5 = buf[j];
			size -= chunk;
		}
	}

	void Save() const override
	{
		std::array<byte, MAP_SL_BUF_SIZE> buf;
		size_t size = _m.GetTileCount();

		SlSetLength(size);
		Map::Iterator i = Map::Iterate().begin();
		while (size > 0) {
			size_t chunk = std::min(size, buf.size());
			for (uint j = 0; j < chunk; ++j, ++i) buf[j] = i->m5;
			SlCopy(buf.data(), chunk, SLE_UINT8);
			size -= chunk;
		}
	}
};

struct MAPEChunkHandler : ChunkHandler {
	MAPEChunkHandler() : ChunkHandler('MAPE', CH_RIFF) {}

	void Load() const override
	{
		std::array<byte, MAP_SL_BUF_SIZE> buf;
		size_t size = _m.GetTileCount();

		if (IsSavegameVersionBefore(SLV_42)) {
			Map::Iterator i = Map::Iterate().begin();
			while (size > 0) {
				size_t chunk = std::min(size, buf.size());
				/* There are four tiles packed into one byte! */
				SlCopy(buf.data(), chunk / 4, SLE_UINT8);
				for (uint j = 0; j < chunk / 4; ++j) {
					(i++)->m6 = GB(buf[j], 0, 2);
					(i++)->m6 = GB(buf[j], 2, 2);
					(i++)->m6 = GB(buf[j], 4, 2);
					(i++)->m6 = GB(buf[j], 6, 2);
					}
				size -= chunk;
			}
		} else {
			Map::Iterator i = Map::Iterate().begin();
			while (size > 0) {
				size_t chunk = std::min(size, buf.size());
				SlCopy(buf.data(), chunk, SLE_UINT8);
				for (uint j = 0; j < chunk; ++j, ++i) i->m6 = buf[j];
				size -= chunk;
			}
		}
	}

	void Save() const override
	{
		std::array<byte, MAP_SL_BUF_SIZE> buf;
		size_t size = _m.GetTileCount();

		SlSetLength(size);
		Map::Iterator i = Map::Iterate().begin();
		while (size > 0) {
			size_t chunk = std::min(size, buf.size());
			for (uint j = 0; j < chunk; ++j, ++i) buf[j] = i->m6;
			SlCopy(buf.data(), chunk, SLE_UINT8);
			size -= chunk;
		}
	}
};

struct MAP7ChunkHandler : ChunkHandler {
	MAP7ChunkHandler() : ChunkHandler('MAP7', CH_RIFF) {}

	void Load() const override
	{
		std::array<byte, MAP_SL_BUF_SIZE> buf;
		size_t size = _m.GetTileCount();

		Map::Iterator i = Map::Iterate().begin();
		while (size > 0) {
			size_t chunk = std::min(size, buf.size());
			SlCopy(buf.data(), chunk, SLE_UINT8);
			for (uint j = 0; j < chunk; ++j, ++i) i->m7 = buf[j];
			size -= chunk;
		}
	}

	void Save() const override
	{
		std::array<byte, MAP_SL_BUF_SIZE> buf;
		size_t size = _m.GetTileCount();

		SlSetLength(size);
		Map::Iterator i = Map::Iterate().begin();
		while (size > 0) {
			size_t chunk = std::min(size, buf.size());
			for (uint j = 0; j < chunk; ++j, ++i) buf[j] = i->m7;
			SlCopy(buf.data(), chunk, SLE_UINT8);
			size -= chunk;
		}
	}
};

struct MAP8ChunkHandler : ChunkHandler {
	MAP8ChunkHandler() : ChunkHandler('MAP8', CH_RIFF) {}

	void Load() const override
	{
		std::array<uint16, MAP_SL_BUF_SIZE> buf;
		size_t size = _m.GetTileCount();

		Map::Iterator i = Map::Iterate().begin();
		while (size > 0) {
			size_t chunk = std::min(size, buf.size());
			SlCopy(buf.data(), chunk, SLE_UINT16);
			for (uint j = 0; j < chunk; ++j, ++i) i->m8 = buf[j];
			size -= chunk;
		}
	}

	void Save() const override
	{
		std::array<uint16, MAP_SL_BUF_SIZE> buf;
		size_t size = _m.GetTileCount();

		SlSetLength(size * sizeof(uint16));
		Map::Iterator i = Map::Iterate().begin();
		while (size > 0) {
			size_t chunk = std::min(size, buf.size());
			for (uint j = 0; j < chunk; ++j, ++i) buf[j] = i->m8;
			SlCopy(buf.data(), chunk, SLE_UINT16);
			size -= chunk;
		}
	}
};

static const MAPSChunkHandler MAPS;
static const MAPRChunkHandler MAPR;
static const MAPTChunkHandler MAPT;
static const MAPHChunkHandler MAPH;
static const MAPOChunkHandler MAPO;
static const MAP2ChunkHandler MAP2;
static const M3LOChunkHandler M3LO;
static const M3HIChunkHandler M3HI;
static const MAP5ChunkHandler MAP5;
static const MAPEChunkHandler MAPE;
static const MAP7ChunkHandler MAP7;
static const MAP8ChunkHandler MAP8;
static const ChunkHandlerRef map_chunk_handlers[] = {
	MAPS,
	MAPR,
	MAPT,
	MAPH,
	MAPO,
	MAP2,
	M3LO,
	M3HI,
	MAP5,
	MAPE,
	MAP7,
	MAP8,
};

extern const ChunkHandlerTable _map_chunk_handlers(map_chunk_handlers);
