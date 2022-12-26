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
#include "window_func.h"
#include "timer/timer.h"
#include "timer/timer_game_calendar.h"
#include <algorithm>
#include <tuple>
#include <array>
#include <numeric>

#include "safeguards.h"

/* Possible link weight modifiers. */
static const byte LWM_ANYWHERE    = 1; ///< Weight modifier for undetermined destinations.
static const byte LWM_TOWN_ANY    = 2; ///< Default weight modifier for towns.
static const byte LWM_TOWN_BIG    = 3; ///< Weight modifier for big towns.
static const byte LWM_TOWN_CITY   = 4; ///< Weight modifier for cities.
static const byte LWM_TOWN_NEARBY = 5; ///< Weight modifier for nearby towns.
static const byte LWM_INTOWN      = 8; ///< Weight modifier for in-town links.
static const byte LWM_IND_ANY     = 2; ///< Default weight modifier for industries.
static const byte LWM_IND_NEARBY  = 3; ///< Weight modifier for nearby industries.
static const byte LWM_IND_PROD    = 4; ///< Weight modifier for producing industries.

static const uint LINK_MIN_WEIGHT = 5; ///< Minimum link weight.

static const uint MAX_EXTRA_LINKS = 2; ///< Number of extra links allowed.
static const uint CITY_TOWN_LINKS = 5; ///< Additional number of links for cities.
static const uint16_t MAX_IND_STOCKPILE = 2048; ///< Maximum stockpile to consider for industry link weight.

/** Population/cargo amount scale divisor for pax/non-pax cargoes for normal tows and big towns. */
static const std::array<uint16_t, 4> POP_SCALE_TOWN{ 200, 100, 1000, 180 };
/** Link weight scale divisor for pax/non-pax cargoes for normal tows and big towns. */
static const std::array<uint, 4> WEIGHT_SCALE_TOWN{ 20, 10, 80, 40 };
/** Cargo amount scale for town and normal cargoes. */
static const std::array<uint16_t, 2> CARGO_SCALE_IND{ 250, 200 };
/** Link weight scale divisor for produced and accepted cargo. */
static const std::array<uint16_t, 2> WEIGHT_SCALE_IND{ 25, 50 };

/** Are fixed cargo destinations enabled for any cargo type? */
static bool AnyFixedCargoDestinations()
{
	return _settings_game.cargo.distribution_pax == DT_FIXED ||
		_settings_game.cargo.distribution_mail == DT_FIXED ||
		_settings_game.cargo.distribution_armoured == DT_FIXED ||
		_settings_game.cargo.distribution_default == DT_FIXED;
}

/** Is this cargo primarily two-way? */
static bool IsSymmetricCargo(CargoID cid)
{
	return IsCargoInClass(cid, CC_PASSENGERS) || IsCargoInClass(cid, CC_MAIL) || (_settings_game.game_creation.landscape == LT_TEMPERATE && IsCargoInClass(cid, CC_ARMOURED));
}

/** Should this cargo type primarily have towns as a destination? */
static bool IsTownCargo(CargoID cid)
{
	const CargoSpec *spec = CargoSpec::Get(cid);
	return spec->town_effect != TE_NONE;
}

/** Remove invalid links from a cargo source/sink. */
static void RemoveInvalidLinks(CargoSourceSink *css)
{
	for (CargoID cid = 0; cid < NUM_CARGO; cid++) {
		/* Remove outgoing links if cargo isn't supplied anymore. */
		if (!css->IsCargoProduced(cid)) {
			for (auto &l : css->cargo_links[cid]) {
				if (l.dest != nullptr) l.dest->num_incoming_links[cid]--;
			}
			css->cargo_links[cid].clear();
			css->cargo_links_weight[cid] = 0;
		}

		/* Remove outgoing links if the dest doesn't accept the cargo anymore. */
		css->cargo_links[cid].erase(std::remove_if(css->cargo_links[cid].begin(), css->cargo_links[cid].end(),
			[=] (const CargoLink &l) {
				if (l.dest != nullptr && !l.dest->IsCargoAccepted(cid)) {
					l.dest->num_incoming_links[cid]--;
					return true;
				}
				return false;
			}), css->cargo_links[cid].end());
	}
}

/** Updated the desired link count for each cargo. */
static void UpdateExpectedLinks(Town* t)
{
	for (CargoID cid : SetCargoBitIterator(t->cargo_produced)) {
		if (_settings_game.cargo.GetDistributionType(cid) == DT_FIXED) {
			t->CreateSpecialLinks(cid);

			bool pax = IsCargoInClass(cid, CC_PASSENGERS);
			uint max_amt = pax ? t->supplied[CT_PASSENGERS].old_max : t->supplied[CT_MAIL].old_max;
			uint big_amt = _settings_game.cargo.yacd.big_town_pop[pax ? 0 : 1];

			uint num_links = _settings_game.cargo.yacd.base_town_links[IsSymmetricCargo(cid) ? 0 : 1];
			/* Add links based on the available cargo amount. */
			num_links += std::min(max_amt, big_amt) / POP_SCALE_TOWN[pax ? 0 : 1];
			if (max_amt > big_amt) num_links += (max_amt - big_amt) / POP_SCALE_TOWN[pax ? 2 : 3];
			/* Ensure a city has at least CITY_TOWN_LINKS more than the base value. This improves
			 * the link distribution at the beginning of a game when the towns are still small. */
			if (t->larger_town) num_links = std::max(num_links, _settings_game.cargo.yacd.base_town_links[IsSymmetricCargo(cid) ? 0 : 1] + CITY_TOWN_LINKS);

			/* Account for the two special links. */
			if (t->cargo_links[cid].size() > 1 && t->cargo_links[cid][1].dest == t) num_links++;
			t->num_links_expected[cid] = ClampTo<uint16_t>(num_links + 1);
		}
	}
}

/** Updated the desired link count for each cargo. */
static void UpdateExpectedLinks(Industry* ind)
{
	for (const auto &p : ind->produced) {
		if (!IsValidCargoID(p.cargo)) continue;

		if (_settings_game.cargo.GetDistributionType(p.cargo) == DT_FIXED) {
			ind->CreateSpecialLinks(p.cargo);

			uint num_links = _settings_game.cargo.yacd.base_ind_links[IsTownCargo(p.cargo) ? 0 : (IsSymmetricCargo(p.cargo) ? 1 : 2)];
			/* Add links based on the average industry production. */
			num_links += p.average / CARGO_SCALE_IND[IsTownCargo(p.cargo) ? 0 : 1];

			ind->num_links_expected[p.cargo] = ClampTo<uint16_t>(num_links + 1); // Account for the one special link.
		}
	}
}

/** Find a supply for a cargo type. */
static CargoSourceSink *FindSupplySource(Industry *dest, CargoID cid)
{
	CargoSourceSink *source = nullptr;

	/* Search for industries before towns. Try for a nearby industry first, then for any industry. */
	for (int i = 0; source == nullptr && i < 2; i++) {
		source = Industry::GetRandom([=] (size_t index) {
				const Industry *ind = Industry::Get(index);
				if (ind == dest) return false;
				if (!ind->IsCargoProduced(cid)) return false;

				if (i == 0 && DistanceSquare(ind->GetXY(), dest->GetXY()) >= Map::ScaleBySize1D(_settings_game.cargo.yacd.ind_nearby_dist)) return false;

				return true;
			});
	}

	if (source == nullptr) {
		/* Try a town. */
		source = Town::GetRandom([=] (size_t index) {
				const Town *t = Town::Get(index);
				return t->IsCargoProduced(cid);
			});
	}

	return source;
}

/** Is there a link to the given destination for a cargo? */
static bool HasLinkTo(const CargoSourceSink *src, const CargoSourceSink *dest, CargoID cid)
{
	return std::find(src->cargo_links[cid].begin(), src->cargo_links[cid].end(), dest) != src->cargo_links[cid].end();
}

/** Make sure an industry has at least one incoming link for each accepted cargo. */
static void AddMissingIndustryLinks(Industry *ind)
{
	for (const auto &p : ind->produced) {
		if (!IsValidCargoID(p.cargo)) continue;

		/* Do we already have at least one cargo source? */
		if (ind->num_incoming_links[p.cargo] > 0) continue;

		CargoSourceSink *source = FindSupplySource(ind, p.cargo);
		if (source == nullptr) continue; // Too bad...

		if (source->cargo_links[p.cargo].size() >= source->num_links_expected[p.cargo] + MAX_EXTRA_LINKS) {
			/* Increase the expected link count if adding another link would
			 * exceed the count, as otherwise this (or another) link would get
			 * removed right again. */
			source->num_links_expected[p.cargo]++;
		}

		source->cargo_links[p.cargo].emplace_back(ind, LWM_IND_ANY);
		ind->num_incoming_links[p.cargo]++;

		/* If this is a symmetric cargo and we produce it as well, create a back link. */
		if (IsSymmetricCargo(p.cargo) && ind->IsCargoProduced(p.cargo) && source->IsCargoAccepted(p.cargo) && !HasLinkTo(ind, source, p.cargo)) {
			ind->cargo_links[p.cargo].emplace_back(source, LWM_IND_ANY);
			source->num_incoming_links[p.cargo]++;
		}
	}
}

/**
 *Remove the link with the lowest weight from a cargo source.The
 *reverse link is removed as well if the cargo has symmetric demand.
 *@param source Remove the link from this cargo source.
 *@param cid Cargo type of the link to remove.
 */
static void RemoveLowestLink(CargoSourceSink *source, CargoID cid)
{
	/* Don't remove special links. */
	if (source->cargo_links[cid].size() < 2) return;
	auto begin = source->cargo_links[cid].begin() + 1;
	if (source->cargo_links[cid].size() >= 2 && source->cargo_links[cid][1].dest == source) begin++;

	auto min = std::min_element(begin, source->cargo_links[cid].end(), [] (const CargoLink &a, const CargoLink &b) {
			return a.weight < b.weight;
		});

	if (min != source->cargo_links[cid].end()) {
		/* If this is a symmetric cargo, also remove the reverse link. */
		if (IsSymmetricCargo(cid) && HasLinkTo(min->dest, source, cid)) {
			source->num_incoming_links[cid]--;
			min->dest->cargo_links[cid].erase(std::remove(min->dest->cargo_links[cid].begin(), min->dest->cargo_links[cid].end(), source), min->dest->cargo_links[cid].end());
		}

		min->dest->num_incoming_links[cid]--;
		source->cargo_links[cid].erase(min);
	}
}

/** Common helper for town/industry enumeration. */
static bool EnumAnyDest(const CargoSourceSink *source, const CargoSourceSink *dest, CargoID cid, bool limit)
{
	/* Destination accepts the cargo at all? */
	if (!dest->IsCargoAccepted(cid)) return false;
	/* Already a destination? */
	if (HasLinkTo(source, dest, cid)) return false;
	/* Destination already has too many links? */
	if (limit && dest->cargo_links[cid].size() > dest->num_links_expected[cid] + MAX_EXTRA_LINKS) return false;

	return true;
}

/** Filter for selecting nearby towns. */
static bool EnumNearbyTown(const CargoSourceSink *source, const Town *t, CargoID)
{
	return DistanceSquare(t->xy, source->GetXY()) < Map::ScaleBySize1D(_settings_game.cargo.yacd.town_nearby_dist);
}

/** Filter for selecting cities. */
static bool EnumCity(const CargoSourceSink *, const Town *t, CargoID)
{
	return t->larger_town;
}

/** Filter for selecting larger towns. */
static bool EnumBigTown(const CargoSourceSink *, const Town *t, CargoID cid)
{
	return IsCargoInClass(cid, CC_PASSENGERS) ? t->supplied[CT_PASSENGERS].old_max > _settings_game.cargo.yacd.big_town_pop[0] : t->supplied[CT_MAIL].old_max > _settings_game.cargo.yacd.big_town_pop[1];
}

/** Find a town as a destination. */
static std::tuple<CargoSourceSink *, byte> FindTownDestination(CargoSourceSink *source, CargoID cid, bool prefer_local)
{
	/* Enum functions for: nearby town, city, big town, and any town. */
	typedef bool (*EnumProc)(const CargoSourceSink *, const Town *, CargoID);
	static const EnumProc destclass_enum[] = {
		&EnumNearbyTown, &EnumCity, &EnumBigTown, nullptr
	};
	static const byte destclass_weight[] = { LWM_TOWN_NEARBY, LWM_TOWN_CITY, LWM_TOWN_BIG, LWM_TOWN_ANY };
	static_assert(lengthof(destclass_enum) == lengthof(destclass_weight));

	TownID self = source->GetType() == SourceType::Town ? (TownID)source->GetID() : INVALID_TOWN;
	bool try_local = prefer_local || Chance16(7, 10);

	/* Try each destination class in order until we find a match. */
	Town *dest = nullptr;
	byte dest_weight = LWM_TOWN_ANY;
	for (int i = try_local ? 0 : 1; dest == nullptr && i < lengthof(destclass_enum); i++) {
		dest_weight = destclass_weight[i];

		dest = Town::GetRandom([=] (size_t index) {
				const Town *t = Town::Get(index);
				if (t->index == self) return false;
				if (!EnumAnyDest(source, t, cid, IsSymmetricCargo(cid))) return false;

				/* Apply filter. */
				if (destclass_enum[i] != nullptr && !destclass_enum[i](source, t, cid)) return false;

				return true;
			});
	}

	return { dest, dest_weight };
}

/** Filter for selecting nearby industries. */
static bool EnumNearbyIndustry(const CargoSourceSink *source, const Industry *ind)
{
	return DistanceSquare(ind->GetXY(), source->GetXY()) < Map::ScaleBySize1D(_settings_game.cargo.yacd.ind_nearby_dist);
}

/** Enumerate industries that are producing cargo. */
static bool EnumProducingIndustry(const CargoSourceSink *, const Industry *ind)
{
	return ind->IsCargoProduced();
}

/** Find an industry as a destination. */
static std::tuple<CargoSourceSink *, byte> FindIndustryDestination(CargoSourceSink *source, CargoID cid)
{
	/* Enum functions for: nearby industries, producing industries, and any industry. */
	typedef bool (*EnumProc)(const CargoSourceSink *, const Industry *);
	static const EnumProc destclass_enum[] = {
		&EnumNearbyIndustry, &EnumProducingIndustry, nullptr
	};
	static const byte destclass_weight[] = { LWM_IND_NEARBY, LWM_IND_PROD, LWM_IND_ANY };
	static_assert(lengthof(destclass_enum) == lengthof(destclass_weight));

	IndustryID self = source->GetType() == SourceType::Industry ? (IndustryID)source->GetID() : INVALID_INDUSTRY;
	bool try_local = Chance16(8, 10);

	/* Try each destination class in order until we find a match. */
	Industry *dest = nullptr;
	byte dest_weight = LWM_IND_ANY;
	for (int i = try_local ? 0 : 1; dest == nullptr && i < lengthof(destclass_enum); i++) {
		dest_weight = destclass_weight[i];
		dest = Industry::GetRandom([=] (size_t index) {
				const Industry *ind = Industry::Get(index);
				if (ind->index == self) return false;
				if (!EnumAnyDest(source, ind, cid, IsSymmetricCargo(cid))) return false;

				/* Apply filter. */
				if (destclass_enum[i] != nullptr && !destclass_enum[i](source, ind)) return false;

				return  true;
			});
	}

	return { dest, dest_weight };
}

/**
 * Create missing cargo links for a source.
 * @param source Source to create the link for.
 * @param cid Cargo type to create the link for.
 * @param chance_a The nominator of the chance for town destinations.
 * @param chance_b The denominator of the chance for town destinations.
 * @param prefer_local Prefer creating local links first, e.g. for small towns.
 */
static void CreateNewLinks(CargoSourceSink *source, CargoID cid, uint chance_a, uint chance_b, bool prefer_local)
{
	uint num_links = source->num_links_expected[cid];

	/* Remove the link with the lowest weight if the source has more than expected. */
	if (source->cargo_links[cid].size() > num_links + MAX_EXTRA_LINKS) {
		RemoveLowestLink(source, cid);
	}

	/* Add new links until the expected link count is reached. */
	while (source->cargo_links[cid].size() < num_links) {
		CargoSourceSink *dest = nullptr;
		byte dest_weight = LWM_ANYWHERE;

		/* Chance for town first is chance_a/chance_b, otherwise try industry first. */
		if (Chance16(chance_a, chance_b)) {
			std::tie(dest, dest_weight) = FindTownDestination(source, cid, prefer_local);
			if (dest == nullptr) std::tie(dest, dest_weight) = FindIndustryDestination(source, cid);
		} else {
			std::tie(dest, dest_weight) = FindIndustryDestination(source, cid);
			if (dest == nullptr) std::tie(dest, dest_weight) = FindTownDestination(source, cid, prefer_local);
		}

		/* If we didn't find a destination, break out of the loop because no
		 * more destinations are left on the map. */
		if (dest == nullptr) break;

		/* If this is a symmetric cargo and we accept it as well, create a back link. */
		if (IsSymmetricCargo(cid) && dest->IsCargoProduced(cid) && source->IsCargoAccepted(cid) && !HasLinkTo(dest, source, cid)) {
			dest->cargo_links[cid].emplace_back(source, dest_weight);
			source->num_incoming_links[cid]++;
		}

		dest->num_incoming_links[cid]++;
		source->cargo_links[cid].emplace_back(dest, dest_weight);
	}
}

/** Try to ensure a town has at least one link to a nearby city. */
static void AddCityLink(Town *source)
{
	if (source->larger_town) return; // Skip cities.

	for (CargoID cid : SetCargoBitIterator(source->cargo_produced)) {
		if (_settings_game.cargo.GetDistributionType(cid) == DT_FIXED) {
			/* Enough links already? */
			if (source->cargo_links[cid].size() >= source->num_links_expected[cid]) continue;

			/* Is there already a city link? */
			auto has_city = [] (const CargoLink &l) { return l.dest != nullptr && l.dest->GetType() == SourceType::Town && static_cast<Town *>(l.dest)->larger_town; };
			if (std::any_of(source->cargo_links[cid].begin(), source->cargo_links[cid].end(), has_city)) continue;

			/* Try to find a nearby city. */
			Town *dest = Town::GetRandom([=] (size_t index) {
				const Town *t = Town::Get(index);
				if (t == source) return false;
				if (!EnumAnyDest(source, t, cid, IsSymmetricCargo(cid))) return false;

				/* Filter for nearby cities. */
				return EnumCity(source, t, cid) && EnumNearbyTown(source, t, cid);
			});

			if (dest == nullptr) continue; // No good destination? Too bad...

			/* If this is a symmetric cargo and we accept it as well, create a back link. */
			if (IsSymmetricCargo(cid) && dest->IsCargoProduced(cid) && source->IsCargoAccepted(cid) && !HasLinkTo(dest, source, cid)) {
				dest->cargo_links[cid].emplace_back(source, LWM_TOWN_CITY);
				source->num_incoming_links[cid]++;
			}

			source->cargo_links[cid].emplace_back(dest, LWM_TOWN_CITY);
		}
	}
}

/** Update the demand links. */
static void UpdateCargoLinks(Town *t)
{
	for (CargoID cid : SetCargoBitIterator(t->cargo_produced)) {
		if (_settings_game.cargo.GetDistributionType(cid) == DT_FIXED) {
			/* If this is a town cargo, 95% chance for town/industry destination
			 * and 5% for industry/town. The reverse chance otherwise. */
			CreateNewLinks(t, cid, IsTownCargo(cid) ? 19 : 1, 20, !t->larger_town);
		}
	}
}

/** Update the demand links. */
static void UpdateCargoLinks(Industry *ind)
{
	for (const auto &p : ind->produced) {
		if (!IsValidCargoID(p.cargo)) continue;

		if (_settings_game.cargo.GetDistributionType(p.cargo) == DT_FIXED) {
			/* If this is a town cargo, 75% chance for town/industry destination
			 * and 25% for industry/town. The reverse chance otherwise. */
			CreateNewLinks(ind, p.cargo, IsTownCargo(p.cargo) ? 3 : 1, 4, true);
		}
	}
}

/** Recalculate the link weights. */
static void UpdateLinkWeights(CargoSourceSink *css)
{
	for (CargoID cid = 0; cid < NUM_CARGO; cid++) {
		css->cargo_links_weight[cid] = 0;
		if (css->cargo_links[cid].empty()) continue;

		uint weight_sum = 0;
		for (auto &l : css->cargo_links[cid]) {
			if (l.dest == nullptr) continue; // Skip the special link for undetermined destinations.

			l.weight = l.dest->GetDestinationWeight(cid, l.weight_mod);
			weight_sum += l.weight;

			l.amount.NewMonth();
		}

		/* Limit the weight of the in-town link to at most 1/3 of the total weight. */
		if (css->cargo_links[cid].size() > 1 && css->cargo_links[cid][1].dest == css) {
			uint new_weight = std::min<uint>(css->cargo_links[cid][1].weight, weight_sum / 3);
			weight_sum -= css->cargo_links[cid][1].weight - new_weight;
			css->cargo_links[cid][1].weight = new_weight;
		}

		/* Set weight for the undetermined destination link to random_dest_chance%. */
		css->cargo_links[cid].front().weight = weight_sum == 0 ? 1 : (weight_sum * _settings_game.cargo.yacd.random_dest_chance) / (100 - _settings_game.cargo.yacd.random_dest_chance);
		css->cargo_links_weight[cid] = weight_sum + css->cargo_links[cid].front().weight;
	}
}

/** Update the demand links of all towns and industries. */
void UpdateCargoLinks()
{
	if (!AnyFixedCargoDestinations()) return;

	/* Remove links that have become invalid. */
	for (Town *t : Town::Iterate()) RemoveInvalidLinks(t);
	for (Industry *i : Industry::Iterate()) RemoveInvalidLinks(i);

	/* Recalculate the number of expected links. */
	for (Town *t : Town::Iterate()) UpdateExpectedLinks(t);
	for (Industry *i : Industry::Iterate()) UpdateExpectedLinks(i);

	/* Link each town to a nearby city. */
	for (Town *t : Town::Iterate()) AddCityLink(t);
	/* Make sure each industry gets at at least some input cargo. */
	for (Industry *i : Industry::Iterate()) AddMissingIndustryLinks(i);

	/* Update the demand link list. */
	for (Town *t : Town::Iterate()) UpdateCargoLinks(t);
	for (Industry *i : Industry::Iterate()) UpdateCargoLinks(i);

	/* Recalculate links weights. */
	for (Town *t : Town::Iterate()) UpdateLinkWeights(t);
	for (Industry *i : Industry::Iterate()) UpdateLinkWeights(i);

	InvalidateWindowClassesData(WC_TOWN_VIEW, -1);
	InvalidateWindowClassesData(WC_INDUSTRY_VIEW, -1);
}

static IntervalTimer<TimerGameCalendar> _cargodest_monthly({ TimerGameCalendar::MONTH, TimerGameCalendar::Priority::CARGODEST }, [](auto)
{
	UpdateCargoLinks();
});


/* virtual */ CargoSourceSink::~CargoSourceSink()
{
	if (Town::CleaningPool() || Industry::CleaningPool()) return;

	/* Remove all demand links having us as a destination. */
	for (Town *t : Town::Iterate()) {
		for (CargoID cid = 0; cid < NUM_CARGO; cid++) {
			auto to_remove = std::remove(t->cargo_links[cid].begin(), t->cargo_links[cid].end(), this);
			if (to_remove != t->cargo_links[cid].end()) InvalidateWindowData(WC_TOWN_VIEW, t->index, -1);
			t->cargo_links[cid].erase(to_remove, t->cargo_links[cid].end());
		}
	}
	for (Industry *ind : Industry::Iterate()) {
		for (CargoID cid = 0; cid < NUM_CARGO; cid++) {
			auto to_remove = std::remove(ind->cargo_links[cid].begin(), ind->cargo_links[cid].end(), this);
			if (to_remove != ind->cargo_links[cid].end()) InvalidateWindowData(WC_INDUSTRY_VIEW, ind->index, -1);
			ind->cargo_links[cid].erase(to_remove, ind->cargo_links[cid].end());
		}
	}

	/* Decrement incoming link count for all link destinations. */
	for (CargoID cid = 0; cid < NUM_CARGO; cid++) {
		for (auto &l : this->cargo_links[cid]) {
			if (l.dest != nullptr) l.dest->num_incoming_links[cid]--;
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

void CargoSourceSink::CreateSpecialLinks(CargoID cid)
{
	/* First link is for undetermined destinations. */
	if (this->cargo_links[cid].empty() || this->cargo_links[cid][0].dest != nullptr) {
		this->cargo_links[cid].emplace(this->cargo_links[cid].begin(), nullptr, LWM_ANYWHERE);
	}
}

void Town::CreateSpecialLinks(CargoID cid)
{
	CargoSourceSink::CreateSpecialLinks(cid);

	if (this->IsCargoAccepted(cid)) {
		/* Add special link for town-local demand if not already present. Our base
		 * class method guarantees that there will be at least one link in the list. */
		if (this->cargo_links[cid].size() < 2 || this->cargo_links[cid][1].dest != this) {
			/* Insert link at second place. */
			this->cargo_links[cid].emplace(this->cargo_links[cid].begin() + 1, this, LWM_INTOWN);
			this->num_incoming_links[cid]++;
		}
	} else {
		/* Remove link for town-local demand if present. */
		if (this->cargo_links[cid].size() >= 2 && this->cargo_links[cid][1].dest == this) {
			this->cargo_links[cid].erase(this->cargo_links[cid].begin() + 1);
			this->num_incoming_links[cid]--;
		}
	}
}

uint Town::GetDestinationWeight(CargoID cid, byte weight_mod) const
{
	/* Estimate town "size" by looking at either the supplied passengers
	 * or the supplied mail. This gives an economic weight to the town that
	 * is somewhat accurate for cargoes like goods that are accept-only. */
	bool pax = IsCargoInClass(cid, CC_PASSENGERS);
	uint max_amt = pax ? this->supplied[CT_PASSENGERS].old_max : this->supplied[CT_MAIL].old_max;
	uint big_amt = _settings_game.cargo.yacd.big_town_pop[pax ? 0 : 1];

	/* The link weight is calculated by a piecewise function. We start with a predefined
	 * minimum weight and then add the weight for the cargo amount up to the big town
	 * amount. If the amount is more than the big town amount, this is also added to the
	 * weight with a different scale factor to make sure that big towns don't siphon the
	 * cargo away too much from the smaller destinations. */
	uint weight = LINK_MIN_WEIGHT;
	weight += std::min(max_amt, big_amt) * weight_mod / WEIGHT_SCALE_TOWN[pax ? 0 : 1];
	if (max_amt > big_amt) weight += (max_amt - big_amt) / WEIGHT_SCALE_TOWN[pax ? 2 : 3];

	return weight;
}

uint Industry::GetDestinationWeight(CargoID cid, byte weight_mod) const
{
	uint weight = LINK_MIN_WEIGHT;

	for (const auto &a : this->accepted) {
		if (a.cargo != cid) continue;
		/* Empty stockpile means more weight for the link. Stockpiles above a fixed maximum have no further effect. */
		uint16_t stockpile = std::min(a.waiting, MAX_IND_STOCKPILE);
		weight += (MAX_IND_STOCKPILE - stockpile) * weight_mod / WEIGHT_SCALE_IND[1];
	}

	/* Add a weight for the produced cargo. Use the average production here so the
	 * weight isn't fluctuating that much when the input cargo isn't delivered regularly. */
	uint16_t total_prod = std::accumulate(std::begin(this->produced), std::end(this->produced), (uint16_t)0, [](uint16_t val, const auto &p) { return val + p.average; });
	weight += total_prod * weight_mod / WEIGHT_SCALE_IND[0];

	return weight;
}

/** Rebuild the cached count of incoming cargo links. */
void RebuildCargoLinkCounts()
{
	/* Clear incoming link count of all towns and industries. */
	for (Town *t : Town::Iterate()) t->num_incoming_links.fill(0);
	for (Industry *i : Industry::Iterate()) i->num_incoming_links.fill(0);

	/* Count all incoming links. */
	for (Town *t : Town::Iterate()) {
		for (CargoID cid = 0; cid < NUM_CARGO; cid++) {
			for (auto &l : t->cargo_links[cid]) {
				if (l.dest != nullptr) l.dest->num_incoming_links[cid]++;
			}
		}
	}
	for (Industry *i : Industry::Iterate()) {
		for (CargoID cid = 0; cid < NUM_CARGO; cid++) {
			for (auto &l : i->cargo_links[cid]) {
				if (l.dest != nullptr) l.dest->num_incoming_links[cid]++;
			}
		}
	}
}
