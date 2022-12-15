/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cargodest_sl.hpp Code handling saving and loading of cargo destinations. */

#ifndef CARGODEST_SL_HPP
#define CARGODEST_SL_HPP

#include "../cargodest_base.h"
#include "../industry.h"
#include "../town.h"

template <typename Tderiv>
class SlCargoSourceSink : public DefaultSaveLoadHandler<SlCargoSourceSink<Tderiv>, Tderiv> {
	inline static uint32_t _packed_link_dest;
	static_assert(sizeof(SourceID) <= 3);
	static_assert(sizeof(SourceType) == 1);

public:
	inline static const SaveLoad description[] = {
		SLEG_VAR("dest",    _packed_link_dest, SLE_UINT32),
		 SLE_VAR(CargoLink, amount.old_max,    SLE_UINT32),
		 SLE_VAR(CargoLink, amount.new_max,    SLE_UINT32),
		 SLE_VAR(CargoLink, amount.old_act,    SLE_UINT32),
		 SLE_VAR(CargoLink, amount.new_act,    SLE_UINT32),
		 SLE_VAR(CargoLink, weight,            SLE_UINT16),
		 SLE_VAR(CargoLink, weight_mod,        SLE_UINT8),
	};

	SaveLoadCompatTable compat_description{};

	void Save(Tderiv *css) const override
	{
		SlSetStructListLength(css->cargo_links.size());

		for (auto &link_list : css->cargo_links) {
			SlSetStructListLength(link_list.size());
			for (CargoLink &link : link_list) {
				/* Pack type and destination index into temp variable. */
				SourceID dest = link.dest != nullptr ? link.dest->GetID() : INVALID_SOURCE;
				SourceType type = link.dest != nullptr ? link.dest->GetType() : SourceType::Industry;
				_packed_link_dest = static_cast<byte>(type) | (static_cast<uint32_t>(dest) << 8);

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

			for (CargoLink &link : css->cargo_links[i]) {
				SlObject(&link, this->GetLoadDescription());

				/* Destination is unpacked later in FixPointers. */
				link.dest = reinterpret_cast<CargoSourceSink *>(static_cast<size_t>(_packed_link_dest));
			}
		}
	}

	void FixPointers(Tderiv *css) const override
	{
		/* Unpack link destination. */
		for (auto &link_list : css->cargo_links) {
			for (CargoLink &link : link_list) {
				/* Extract type and destination index. */
				SourceType type = static_cast<SourceType>(reinterpret_cast<size_t>(link.dest) & 0xFF);
				SourceID dest = static_cast<SourceID>(reinterpret_cast<size_t>(link.dest) >> 8);

				if (dest != INVALID_SOURCE) {
					switch (type) {
						case SourceType::Industry:
							if (!Industry::IsValidID(dest)) SlErrorCorrupt("Invalid cargo link destination");
							link.dest = Industry::Get(dest);
							break;

						case SourceType::Town:
							if (!Town::IsValidID(dest)) SlErrorCorrupt("Invalid cargo link destination");
							link.dest = Town::Get(dest);
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

#endif /* CARGODEST_SL_HPP */
