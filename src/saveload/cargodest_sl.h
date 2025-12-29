/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file cargodest_sl.h Code handling saving and loading of cargo destinations. */

#ifndef CARGODEST_SL_H
#define CARGODEST_SL_H

#include "saveload.h"
#include "../cargodest_base.h"
#include "../industry.h"
#include "../town.h"

template <CargoSourceSinkObject Tderiv>
class SlCargoSourceSink : public DefaultSaveLoadHandler<SlCargoSourceSink<Tderiv>, Tderiv> {
	inline static Source _link_dest;

public:
	inline static const SaveLoad description[] = {
		SLEG_VAR("dest_type", _link_dest.type, SLE_UINT8),
		SLEG_VAR("dest", _link_dest.id, SLE_UINT16),
		SLE_VAR(CargoDemandLink, amount.old_max, SLE_UINT32),
		SLE_VAR(CargoDemandLink, amount.new_max, SLE_UINT32),
		SLE_VAR(CargoDemandLink, amount.old_act, SLE_UINT32),
		SLE_VAR(CargoDemandLink, amount.new_act, SLE_UINT32),
		SLE_VAR(CargoDemandLink, weight, SLE_UINT16),
		SLE_VAR(CargoDemandLink, weight_mod, SLE_UINT8),
	};

	SaveLoadCompatTable compat_description{};

	void Save(Tderiv *css) const override
	{
		SlSetStructListLength(css->cargo_links.size());

		for (auto &link_list : css->cargo_links) {
			SlSetStructListLength(link_list.size());

			for (CargoDemandLink &link : link_list) {
				_link_dest = link.dest != nullptr ? link.dest->ToSource() : Source{Source::Invalid, SourceType::Industry};
				SlObject(&link, this->GetDescription());
			}
		}
	}

	void Load(Tderiv *css) const override
	{
		size_t cids = SlGetStructListLength(css->cargo_links.size());

		for (size_t i = 0; i < cids; i++) {
			size_t elements = SlGetStructListLength(css->cargo_links[i].max_size());
			css->cargo_links[i].resize(elements);

			for (CargoDemandLink &link : css->cargo_links[i]) {
				SlObject(&link, this->GetLoadDescription());

				/* Destination is unpacked later in FixPointers. */
				link.dest = reinterpret_cast<CargoSourceSink *>(_link_dest.Pack());
			}
		}
	}

	void FixPointers(Tderiv *css) const override
	{
		/* Unpack link destination. */
		for (auto &link_list : css->cargo_links) {
			for (CargoDemandLink &link : link_list) {
				Source dest = Source::Unpack(reinterpret_cast<size_t>(link.dest));
				if (dest.IsValid()) {
					switch (dest.type) {
						case SourceType::Industry:
							if (!Industry::IsValidID(dest.id)) SlErrorCorrupt("Invalid cargo link destination");
							link.dest = Industry::Get(dest.ToIndustryID());
							break;

						case SourceType::Town:
							if (!Town::IsValidID(dest.id)) SlErrorCorrupt("Invalid cargo link destination");
							link.dest = Town::Get(dest.ToTownID());
							break;

						default:
							SlErrorCorrupt("Invalid cargo link destination type");
					}
				} else {
					link.dest = nullptr;
				}
			}
		}

		css->UpdateLinkWeightSums();
	}
};

#endif /* CARGODEST_SL_H */
