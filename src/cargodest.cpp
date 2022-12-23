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

#include "safeguards.h"

/* Possible link weight modifiers. */
static const byte LWM_ANYWHERE = 1;    ///< Weight modifier for undetermined destinations.
static const byte LWM_INTOWN = 8;      ///< Weight modifier for in-town links.

static const uint MAX_EXTRA_LINKS = 2; ///< Number of extra links allowed.

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

/** Updated the desired link count for each cargo. */
static void UpdateExpectedLinks(Town* t)
{
	for (CargoID cid : SetCargoBitIterator(t->cargo_produced)) {
		if (_settings_game.cargo.GetDistributionType(cid) == DT_FIXED) {
			t->CreateSpecialLinks(cid);

			uint num_links = _settings_game.cargo.yacd.base_town_links[IsSymmetricCargo(cid) ? 0 : 1];

			/* Account for the two special links. */
			if (t->cargo_links[cid].size() > 1 && t->cargo_links[cid][1].dest == t) num_links++;
			t->num_links_expected[cid] = ClampToU16(num_links + 1);
		}
	}
}

/** Updated the desired link count for each cargo. */
static void UpdateExpectedLinks(Industry* ind)
{
	for (CargoID cid : ind->produced_cargo) {
		if (!IsCargoIDValid(cid)) continue;

		if (_settings_game.cargo.GetDistributionType(cid) == DT_FIXED) {
			ind->CreateSpecialLinks(cid);

			uint num_links = _settings_game.cargo.yacd.base_ind_links[IsTownCargo(cid) ? 0 : 1];
			ind->num_links_expected[cid] = ClampToU16(num_links + 1); // Account for the one special link.
		}
	}
}

/** Is there a link to the given destination for a cargo? */
static bool HasLinkTo(const CargoSourceSink *src, const CargoSourceSink *dest, CargoID cid)
{
	return std::find(src->cargo_links[cid].begin(), src->cargo_links[cid].end(), dest) != src->cargo_links[cid].end();
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
			min->dest->cargo_links[cid].erase(std::remove(min->dest->cargo_links[cid].begin(), min->dest->cargo_links[cid].end(), source), min->dest->cargo_links[cid].end());
		}

		source->cargo_links[cid].erase(min);
	}
}

/** Common helper for town/industry enumeration. */
static bool EnumAnyDest(const CargoSourceSink *source, const CargoSourceSink *dest, CargoID cid, bool limit)
{
	/* Destination accepts the cargo at all? */
	if (!dest->AcceptsCargo(cid)) return false;
	/* Already a destination? */
	if (HasLinkTo(source, dest, cid)) return false;
	/* Destination already has too many links? */
	if (limit && dest->cargo_links[cid].size() > dest->num_links_expected[cid] + MAX_EXTRA_LINKS) return false;

	return true;
}

/** Find a town as a destination. */
static CargoSourceSink *FindTownDestination(CargoSourceSink *source, CargoID cid)
{
	TownID self = source->GetType() == ST_TOWN ? (TownID)source->GetID() : INVALID_TOWN;

	return Town::GetRandom([=] (size_t index) {
			const Town *t = Town::Get(index);
			if (t->index == self) return false;
			return EnumAnyDest(source, t, cid, IsSymmetricCargo(cid));
		});
}

/** Find an industry as a destination. */
static CargoSourceSink *FindIndustryDestination(CargoSourceSink *source, CargoID cid)
{
	IndustryID self = source->GetType() == ST_INDUSTRY ? (IndustryID)source->GetID() : INVALID_INDUSTRY;

	return Industry::GetRandom([=] (size_t index) {
			const Industry *ind = Industry::Get(index);
			if (ind->index == self) return false;
			return EnumAnyDest(source, ind, cid, IsSymmetricCargo(cid));
		});
}

/**
 * Create missing cargo links for a source.
 * @param source Source to create the link for.
 * @param cid Cargo type to create the link for.
 * @param chance_a The nominator of the chance for town destinations.
 * @param chance_b The denominator of the chance for town destinations.
 */
static void CreateNewLinks(CargoSourceSink *source, CargoID cid, uint chance_a, uint chance_b)
{
	uint num_links = source->num_links_expected[cid];

	/* Remove the link with the lowest weight if the source has more than expected. */
	if (source->cargo_links[cid].size() > num_links + MAX_EXTRA_LINKS) {
		RemoveLowestLink(source, cid);
	}

	/* Add new links until the expected link count is reached. */
	while (source->cargo_links[cid].size() < num_links) {
		CargoSourceSink *dest = nullptr;

		/* Chance for town first is chance_a/chance_b, otherwise try industry first. */
		if (Chance16(chance_a, chance_b)) {
			dest = FindTownDestination(source, cid);
			if (dest == nullptr) dest = FindIndustryDestination(source, cid);
		} else {
			dest = FindIndustryDestination(source, cid);
			if (dest == nullptr) dest = FindTownDestination(source, cid);
		}

		/* If we didn't find a destination, break out of the loop because no
		 * more destinations are left on the map. */
		if (dest == nullptr) break;

		/* If this is a symmetric cargo and we accept it as well, create a back link. */
		if (IsSymmetricCargo(cid) && dest->SuppliesCargo(cid) && source->AcceptsCargo(cid) && !HasLinkTo(dest, source, cid)) {
			dest->cargo_links[cid].emplace_back(source, LWM_ANYWHERE);
		}

		source->cargo_links[cid].emplace_back(dest, LWM_ANYWHERE);
	}
}

/** Update the demand links. */
static void UpdateCargoLinks(Town *t)
{
	for (CargoID cid : SetCargoBitIterator(t->cargo_produced)) {
		if (_settings_game.cargo.GetDistributionType(cid) == DT_FIXED) {
			/* If this is a town cargo, 95% chance for town/industry destination
			 * and 5% for industry/town. The reverse chance otherwise. */
			CreateNewLinks(t, cid, IsTownCargo(cid) ? 19 : 1, 20);
		}
	}
}

/** Update the demand links. */
static void UpdateCargoLinks(Industry *ind)
{
	for (CargoID cid : ind->produced_cargo) {
		if (!IsCargoIDValid(cid)) continue;

		if (_settings_game.cargo.GetDistributionType(cid) == DT_FIXED) {
			/* If this is a town cargo, 75% chance for town/industry destination
			 * and 25% for industry/town. The reverse chance otherwise. */
			CreateNewLinks(ind, cid, IsTownCargo(cid) ? 3 : 1, 4);
		}
	}
}

/** Recalculate the link weights. */
static void UpdateLinkWeights(CargoSourceSink *css)
{
	for (CargoID cid = 0; cid < NUM_CARGO; cid++) {
		css->cargo_links_weight[cid] = 0;

		for (auto &l : css->cargo_links[cid]) {
			css->cargo_links_weight[cid] += l.weight;
			l.amount.NewMonth();
		}
	}
}

/** Update the demand links of all towns and industries. */
void UpdateCargoLinks()
{
	if (!AnyFixedCargoDestinations()) return;

	/* Recalculate the number of expected links. */
	for (Town *t : Town::Iterate()) UpdateExpectedLinks(t);
	for (Industry *i : Industry::Iterate()) UpdateExpectedLinks(i);

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

	if (this->AcceptsCargo(cid)) {
		/* Add special link for town-local demand if not already present. Our base
		 * class method guarantees that there will be at least one link in the list. */
		if (this->cargo_links[cid].size() < 2 || this->cargo_links[cid][1].dest != this) {
			/* Insert link at second place. */
			this->cargo_links[cid].emplace(this->cargo_links[cid].begin() + 1, this, LWM_INTOWN);
		}
	} else {
		/* Remove link for town-local demand if present. */
		if (this->cargo_links[cid].size() >= 2 && this->cargo_links[cid][1].dest == this) this->cargo_links[cid].erase(this->cargo_links[cid].begin() + 1);
	}
}
