/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file rail_map.h Hides the direct accesses to the map array with map accessors */

#ifndef RAIL_MAP_H
#define RAIL_MAP_H

#include "rail_type.h"
#include "depot_type.h"
#include "signal_func.h"
#include "track_func.h"
#include "tile_map.h"
#include "water_map.h"
#include "signal_type.h"


/** Different types of Rail-related tiles */
enum RailTileType {
	RAIL_TILE_NORMAL   = 0, ///< Normal rail tile without signals
	RAIL_TILE_SIGNALS  = 1, ///< Normal rail tile with signals
	RAIL_TILE_DEPOT    = 3, ///< Depot (one entrance)
};

/**
 * Returns the RailTileType (normal with or without signals,
 * waypoint or depot).
 * @param t the tile to get the information from
 * @pre IsTileType(t, MP_RAILWAY)
 * @return the RailTileType
 */
debug_inline static RailTileType GetRailTileType(Tile t)
{
	assert(IsTileType(t, MP_RAILWAY));
	return (RailTileType)GB(t.m5(), 6, 2);
}

/**
 * Returns whether this is plain rails, with or without signals. Iow, if this
 * tiles RailTileType is RAIL_TILE_NORMAL or RAIL_TILE_SIGNALS.
 * @param t the tile to get the information from
 * @pre IsTileType(t, MP_RAILWAY)
 * @return true if and only if the tile is normal rail (with or without signals)
 */
debug_inline static bool IsPlainRail(Tile t)
{
	RailTileType rtt = GetRailTileType(t);
	return rtt == RAIL_TILE_NORMAL || rtt == RAIL_TILE_SIGNALS;
}

/**
 * Checks whether the tile is a rail tile or rail tile with signals.
 * @param t the tile to get the information from
 * @return true if and only if the tile is normal rail (with or without signals)
 */
debug_inline static bool IsPlainRailTile(Tile t)
{
	return t.IsValid() && IsTileType(t, MP_RAILWAY) && IsPlainRail(t);
}

/**
 * Checks whether the tile is a rail tile or rail tile with signals.
 * @param t the tile to get the information from
 * @return true if and only if the tile is normal rail (with or without signals)
 */
debug_inline static bool IsPlainRailTile(TileIndex t)
{
	return IsPlainRailTile(Tile::GetByType(t, MP_RAILWAY));
}

/**
 * Checks if a rail tile has signals.
 * @param t the tile to get the information from
 * @pre IsTileType(t, MP_RAILWAY)
 * @return true if and only if the tile has signals
 */
inline bool HasSignals(Tile t)
{
	return GetRailTileType(t) == RAIL_TILE_SIGNALS;
}

/**
 * Add/remove the 'has signal' bit from the RailTileType
 * @param tile the tile to add/remove the signals to/from
 * @param signals whether the rail tile should have signals or not
 * @pre IsPlainRailTile(tile)
 */
inline void SetHasSignals(Tile tile, bool signals)
{
	assert(IsPlainRailTile(tile));
	AssignBit(tile.m5(), 6, signals);
}

/**
 * Is this rail tile a rail depot?
 * @param t the tile to get the information from
 * @pre IsTileType(t, MP_RAILWAY)
 * @return true if and only if the tile is a rail depot
 */
debug_inline static bool IsRailDepot(Tile t)
{
	return GetRailTileType(t) == RAIL_TILE_DEPOT;
}

/**
 * Is this tile rail tile and a rail depot?
 * @param t the tile to get the information from
 * @return true if and only if the tile is a rail depot
 */
debug_inline static bool IsRailDepotTile(Tile t)
{
	return t.IsValid() && IsTileType(t, MP_RAILWAY) && IsRailDepot(t);
}

/**
 * Is this tile rail tile and a rail depot?
 * @param t the tile to get the information from
 * @return true if and only if the tile is a rail depot
 */
debug_inline static bool IsRailDepotTile(TileIndex t)
{
	return IsRailDepotTile(Tile::GetByType(t, MP_RAILWAY));
}

/**
 * Gets the rail type of the given tile
 * @param t the tile to get the rail type from
 * @return the rail type of the tile
 */
inline RailType GetRailType(Tile t)
{
	return (RailType)GB(t.m8(), 0, 6);
}

/**
 * Sets the rail type of the given tile
 * @param t the tile to set the rail type of
 * @param r the new rail type for the tile
 */
inline void SetRailType(Tile t, RailType r)
{
	SB(t.m8(), 0, 6, r);
}


/**
 * Gets the track bits of the given tile
 * @param tile the tile to get the track bits from
 * @return the track bits of the tile
 */
inline TrackBits GetTrackBits(Tile tile)
{
	assert(IsPlainRailTile(tile));
	return (TrackBits)GB(tile.m5(), 0, 6);
}

/**
 * Sets the track bits of the given tile
 * @param t the tile to set the track bits of
 * @param b the new track bits for the tile
 */
inline void SetTrackBits(Tile t, TrackBits b)
{
	assert(IsPlainRailTile(t));
	SB(t.m5(), 0, 6, b);
}

/**
 * Returns whether the given track is present on the given tile.
 * @param tile  the tile to check the track presence of
 * @param track the track to search for on the tile
 * @pre IsPlainRailTile(tile)
 * @return true if and only if the given track exists on the tile
 */
inline bool HasTrack(Tile tile, Track track)
{
	return HasBit(GetTrackBits(tile), track);
}

/**
 * Returns the direction the depot is facing to
 * @param t the tile to get the depot facing from
 * @pre IsRailDepotTile(t)
 * @return the direction the depot is facing
 */
inline DiagDirection GetRailDepotDirection(Tile t)
{
	return (DiagDirection)GB(t.m5(), 0, 2);
}

/**
 * Returns the track of a depot, ignoring direction
 * @pre IsRailDepotTile(t)
 * @param t the tile to get the depot track from
 * @return the track of the depot
 */
inline Track GetRailDepotTrack(Tile t)
{
	return DiagDirToDiagTrack(GetRailDepotDirection(t));
}

/**
 * Get the actual associated sub-tile of a rail depot.
 * @pre IsRailDepotTile(index)
 * @param index The tile index to get the depot tile for.
 * @return The depot sub-tile.
 */
inline Tile GetRailDepotTile(TileIndex index)
{
	assert(IsRailDepotTile(index));
	return Tile::GetByType(index, MP_RAILWAY);
}


/**
 * Returns the reserved track bits of the tile
 * @pre IsPlainRailTile(t)
 * @param t the tile to query
 * @return the track bits
 */
inline TrackBits GetRailReservationTrackBits(Tile t)
{
	assert(IsPlainRailTile(t));
	uint8_t track_b = GB(t.m2(), 8, 3);
	Track track = (Track)(track_b - 1);    // map array saves Track+1
	if (track_b == 0) return TRACK_BIT_NONE;
	return (TrackBits)(TrackToTrackBits(track) | (HasBit(t.m2(), 11) ? TrackToTrackBits(TrackToOppositeTrack(track)) : 0));
}

/**
 * Sets the reserved track bits of the tile
 * @pre IsPlainRailTile(t) && !TracksOverlap(b)
 * @param t the tile to change
 * @param b the track bits
 */
inline void SetTrackReservation(Tile t, TrackBits b)
{
	assert(IsPlainRailTile(t));
	assert(b != INVALID_TRACK_BIT);
	assert(!TracksOverlap(b));
	Track track = RemoveFirstTrack(&b);
	SB(t.m2(), 8, 3, track == INVALID_TRACK ? 0 : track + 1);
	AssignBit(t.m2(), 11, b != TRACK_BIT_NONE);
}

/**
 * Try to reserve a specific track on a tile
 * @pre IsPlainRailTile(t) && HasTrack(tile, t)
 * @param tile the tile
 * @param t the rack to reserve
 * @return true if successful
 */
inline bool TryReserveTrack(Tile tile, Track t)
{
	assert(HasTrack(tile, t));
	TrackBits bits = TrackToTrackBits(t);
	TrackBits res = GetRailReservationTrackBits(tile);
	if ((res & bits) != TRACK_BIT_NONE) return false;  // already reserved
	res |= bits;
	if (TracksOverlap(res)) return false;  // crossing reservation present
	SetTrackReservation(tile, res);
	return true;
}

/**
 * Lift the reservation of a specific track on a tile
 * @pre IsPlainRailTile(t) && HasTrack(tile, t)
 * @param tile the tile
 * @param t the track to free
 */
inline void UnreserveTrack(Tile tile, Track t)
{
	assert(HasTrack(tile, t));
	TrackBits res = GetRailReservationTrackBits(tile);
	res &= ~TrackToTrackBits(t);
	SetTrackReservation(tile, res);
}

/**
 * Get the reservation state of the depot
 * @pre IsRailDepot(t)
 * @param t the depot tile
 * @return reservation state
 */
inline bool HasDepotReservation(Tile t)
{
	assert(IsRailDepot(t));
	return HasBit(t.m5(), 4);
}

/**
 * Set the reservation state of the depot
 * @pre IsRailDepot(t)
 * @param t the depot tile
 * @param b the reservation state
 */
inline void SetDepotReservation(Tile t, bool b)
{
	assert(IsRailDepot(t));
	AssignBit(t.m5(), 4, b);
}

/**
 * Get the reserved track bits for a depot
 * @pre IsRailDepot(t)
 * @param t the tile
 * @return reserved track bits
 */
inline TrackBits GetDepotReservationTrackBits(Tile t)
{
	return HasDepotReservation(t) ? TrackToTrackBits(GetRailDepotTrack(t)) : TRACK_BIT_NONE;
}


inline bool IsPbsSignal(SignalType s)
{
	return s == SIGTYPE_PBS || s == SIGTYPE_PBS_ONEWAY;
}

inline SignalType GetSignalType(Tile t, Track track)
{
	assert(GetRailTileType(t) == RAIL_TILE_SIGNALS);
	uint8_t pos = (track == TRACK_LOWER || track == TRACK_RIGHT) ? 4 : 0;
	return (SignalType)GB(t.m2(), pos, 3);
}

inline void SetSignalType(Tile t, Track track, SignalType s)
{
	assert(GetRailTileType(t) == RAIL_TILE_SIGNALS);
	uint8_t pos = (track == TRACK_LOWER || track == TRACK_RIGHT) ? 4 : 0;
	SB(t.m2(), pos, 3, s);
	if (track == INVALID_TRACK) SB(t.m2(), 4, 3, s);
}

inline bool IsPresignalEntry(Tile t, Track track)
{
	return GetSignalType(t, track) == SIGTYPE_ENTRY || GetSignalType(t, track) == SIGTYPE_COMBO;
}

inline bool IsPresignalExit(Tile t, Track track)
{
	return GetSignalType(t, track) == SIGTYPE_EXIT || GetSignalType(t, track) == SIGTYPE_COMBO;
}

/** One-way signals can't be passed the 'wrong' way. */
inline bool IsOnewaySignal(Tile t, Track track)
{
	return GetSignalType(t, track) != SIGTYPE_PBS;
}

inline void CycleSignalSide(Tile t, Track track)
{
	uint8_t sig;
	uint8_t pos = (track == TRACK_LOWER || track == TRACK_RIGHT) ? 4 : 6;

	sig = GB(t.m3(), pos, 2);
	if (--sig == 0) sig = IsPbsSignal(GetSignalType(t, track)) ? 2 : 3;
	SB(t.m3(), pos, 2, sig);
}

inline SignalVariant GetSignalVariant(Tile t, Track track)
{
	uint8_t pos = (track == TRACK_LOWER || track == TRACK_RIGHT) ? 7 : 3;
	return (SignalVariant)GB(t.m2(), pos, 1);
}

inline void SetSignalVariant(Tile t, Track track, SignalVariant v)
{
	uint8_t pos = (track == TRACK_LOWER || track == TRACK_RIGHT) ? 7 : 3;
	SB(t.m2(), pos, 1, v);
	if (track == INVALID_TRACK) SB(t.m2(), 7, 1, v);
}

/**
 * Set the states of the signals (Along/AgainstTrackDir)
 * @param tile  the tile to set the states for
 * @param state the new state
 */
inline void SetSignalStates(Tile tile, uint state)
{
	SB(tile.m4(), 4, 4, state);
}

/**
 * Set the states of the signals (Along/AgainstTrackDir)
 * @param tile  the tile to set the states for
 * @return the state of the signals
 */
inline uint GetSignalStates(Tile tile)
{
	return GB(tile.m4(), 4, 4);
}

/**
 * Get the state of a single signal
 * @param t         the tile to get the signal state for
 * @param signalbit the signal
 * @return the state of the signal
 */
inline SignalState GetSingleSignalState(Tile t, uint8_t signalbit)
{
	return (SignalState)HasBit(GetSignalStates(t), signalbit);
}

/**
 * Set whether the given signals are present (Along/AgainstTrackDir)
 * @param tile    the tile to set the present signals for
 * @param signals the signals that have to be present
 */
inline void SetPresentSignals(Tile tile, uint signals)
{
	SB(tile.m3(), 4, 4, signals);
}

/**
 * Get whether the given signals are present (Along/AgainstTrackDir)
 * @param tile the tile to get the present signals for
 * @return the signals that are present
 */
inline uint GetPresentSignals(Tile tile)
{
	return GB(tile.m3(), 4, 4);
}

/**
 * Checks whether the given signals is present
 * @param t         the tile to check on
 * @param signalbit the signal
 * @return true if and only if the signal is present
 */
inline bool IsSignalPresent(Tile t, uint8_t signalbit)
{
	return HasBit(GetPresentSignals(t), signalbit);
}

/**
 * Checks for the presence of signals (either way) on the given track on the
 * given rail tile.
 */
inline bool HasSignalOnTrack(Tile tile, Track track)
{
	assert(IsValidTrack(track));
	return GetRailTileType(tile) == RAIL_TILE_SIGNALS && (GetPresentSignals(tile) & SignalOnTrack(track)) != 0;
}

/**
 * Checks for the presence of signals along the given trackdir on the given
 * rail tile.
 *
 * Along meaning if you are currently driving on the given trackdir, this is
 * the signal that is facing us (for which we stop when it's red).
 */
inline bool HasSignalOnTrackdir(Tile tile, Trackdir trackdir)
{
	assert (IsValidTrackdir(trackdir));
	return GetRailTileType(tile) == RAIL_TILE_SIGNALS && GetPresentSignals(tile) & SignalAlongTrackdir(trackdir);
}

/**
 * Gets the state of the signal along the given trackdir.
 *
 * Along meaning if you are currently driving on the given trackdir, this is
 * the signal that is facing us (for which we stop when it's red).
 */
inline SignalState GetSignalStateByTrackdir(Tile tile, Trackdir trackdir)
{
	assert(IsValidTrackdir(trackdir));
	assert(HasSignalOnTrack(tile, TrackdirToTrack(trackdir)));
	return GetSignalStates(tile) & SignalAlongTrackdir(trackdir) ?
		SIGNAL_STATE_GREEN : SIGNAL_STATE_RED;
}

/**
 * Sets the state of the signal along the given trackdir.
 */
inline void SetSignalStateByTrackdir(Tile tile, Trackdir trackdir, SignalState state)
{
	if (state == SIGNAL_STATE_GREEN) { // set 1
		SetSignalStates(tile, GetSignalStates(tile) | SignalAlongTrackdir(trackdir));
	} else {
		SetSignalStates(tile, GetSignalStates(tile) & ~SignalAlongTrackdir(trackdir));
	}
}

/**
 * Is a pbs signal present along the trackdir?
 * @param tile the tile to check
 * @param td the trackdir to check
 */
inline bool HasPbsSignalOnTrackdir(Tile tile, Trackdir td)
{
	return tile && IsTileType(tile, MP_RAILWAY) && HasSignalOnTrackdir(tile, td) &&
			IsPbsSignal(GetSignalType(tile, TrackdirToTrack(td)));
}

/**
 * Is a pbs signal present along the trackdir?
 * @param index the tile to check
 * @param td the trackdir to check
 */
inline bool HasPbsSignalOnTrackdir(TileIndex index, Trackdir td)
{
	return HasPbsSignalOnTrackdir(Tile::GetByType(index, MP_RAILWAY), td);
}

/**
 * Is a one-way signal blocking the trackdir? A one-way signal on the
 * trackdir against will block, but signals on both trackdirs won't.
 * @param tile the tile to check
 * @param td the trackdir to check
 */
inline bool HasOnewaySignalBlockingTrackdir(Tile tile, Trackdir td)
{
	return tile && IsTileType(tile, MP_RAILWAY) && HasSignalOnTrackdir(tile, ReverseTrackdir(td)) &&
			!HasSignalOnTrackdir(tile, td) && IsOnewaySignal(tile, TrackdirToTrack(td));
}

/**
 * Is a one-way signal blocking the trackdir? A one-way signal on the
 * trackdir against will block, but signals on both trackdirs won't.
 * @param index the tile to check
 * @param td the trackdir to check
 */
inline bool HasOnewaySignalBlockingTrackdir(TileIndex index, Trackdir td)
{
	return HasOnewaySignalBlockingTrackdir(Tile::GetByType(index, MP_RAILWAY), td);
}

/**
 * Is a block signal present along the trackdir?
 * @param tile the tile to check
 * @param td the trackdir to check
 */
inline bool HasBlockSignalOnTrackdir(Tile tile, Trackdir td)
{
	return tile && IsTileType(tile, MP_RAILWAY) && HasSignalOnTrackdir(tile, td) &&
		!IsPbsSignal(GetSignalType(tile, TrackdirToTrack(td)));
}

/**
 * Is a block signal present along the trackdir?
 * @param index the tile to check
 * @param td the trackdir to check
 */
inline bool HasBlockSignalOnTrackdir(TileIndex index, Trackdir td)
{
	return HasBlockSignalOnTrackdir(Tile::GetByType(index, MP_RAILWAY), td);
}


RailType GetTileRailType(TileIndex tile);

/** The type of fences around the rail. */
enum RailFenceType {
	RAIL_FENCE_NONE   =  0, ///< No fences
	RAIL_FENCE_NW     =  1, ///< Grass with a fence at the NW edge
	RAIL_FENCE_SE     =  2, ///< Grass with a fence at the SE edge
	RAIL_FENCE_SENW   =  3, ///< Grass with a fence at the NW and SE edges
	RAIL_FENCE_NE     =  4, ///< Grass with a fence at the NE edge
	RAIL_FENCE_SW     =  5, ///< Grass with a fence at the SW edge
	RAIL_FENCE_NESW   =  6, ///< Grass with a fence at the NE and SW edges
	RAIL_FENCE_VERT1  =  7, ///< Grass with a fence at the eastern side
	RAIL_FENCE_VERT2  =  8, ///< Grass with a fence at the western side
	RAIL_FENCE_HORIZ1 =  9, ///< Grass with a fence at the southern side
	RAIL_FENCE_HORIZ2 = 10, ///< Grass with a fence at the northern side
};

inline void SetRailFenceType(Tile t, RailFenceType rft)
{
	SB(t.m4(), 0, 4, rft);
}

inline RailFenceType GetRailFenceType(Tile t)
{
	return static_cast<RailFenceType>(GB(t.m4(), 0, 4));
}


inline Tile MakeRailNormal(Tile t, Owner o, TrackBits b, RailType r)
{
	SetTileType(t, MP_RAILWAY);
	SetTileOwner(t, o);
	t.m2() = 0;
	t.m3() = 0;
	t.m4() = 0;
	t.m5() = RAIL_TILE_NORMAL << 6 | b;
	t.m6() = 0;
	t.m7() = 0;
	SetRailType(t, r);
	return t;
}

inline Tile MakeRailNormal(TileIndex index, Owner o, TrackBits b, RailType r)
{
	Tile rail = Tile::New(index, MP_RAILWAY);
	return MakeRailNormal(rail, o, b, r);
}

/**
 * Sets the exit direction of a rail depot.
 * @param tile Tile of the depot.
 * @param dir  Direction of the depot exit.
 */
inline void SetRailDepotExitDirection(Tile tile, DiagDirection dir)
{
	assert(IsRailDepotTile(tile));
	SB(tile.m5(), 0, 2, dir);
}

/**
 * Make a rail depot.
 * @param index     Tile to make a depot on.
 * @param owner     New owner of the depot.
 * @param depot_id  New depot ID.
 * @param dir       Direction of the depot exit.
 * @param rail_type Rail type of the depot.
 */
inline void MakeRailDepot(TileIndex index, Owner owner, DepotID depot_id, DiagDirection dir, RailType rail_type)
{
	Tile tile = Tile::New(index, MP_RAILWAY);
	SetTileOwner(tile, owner);
	tile.m2() = depot_id;
	tile.m3() = 0;
	tile.m4() = 0;
	tile.m5() = RAIL_TILE_DEPOT << 6 | dir;
	tile.m6() = 0;
	tile.m7() = 0;
	SetRailType(tile, rail_type);
}

#endif /* RAIL_MAP_H */
