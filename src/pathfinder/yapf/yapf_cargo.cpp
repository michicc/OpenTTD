/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file yapf_cargo.cpp Implementation of YAPF for cargo routing. */

#include "../../stdafx.h"
#include "../../cargodest_base.h"
#include "../../station_base.h"
#include "../../linkgraph/linkgraph_base.h"

#include "yapf.hpp"
#include <functional>

#include "../../safeguards.h"

/** YAPF node key for cargo routing. */
struct CYapfRouteLinkNodeKeyT {
	ConstEdge *m_edge;

	/** Initialize this node key. */
	inline void Set(ConstEdge *edge)
	{
		this->m_edge = edge;
	}

	/** Calculate the hash of this cargo/route key. */
	inline int CalcHash() const
	{
		return (int)std::hash<ConstEdge *>{}(m_edge);
	}

	inline bool operator == (const CYapfRouteLinkNodeKeyT &other) const
	{
		return this->m_edge == other.m_edge;
	}

	void Dump(DumpTarget &dmp) const
	{
		dmp.WriteValue("m_edge", this->m_edge->dest_node);
	}
};

/** YAPF node class for cargo routing. */
struct CYapfRouteLinkNodeT : public CYapfNodeT<CYapfRouteLinkNodeKeyT, CYapfRouteLinkNodeT> {
	typedef CYapfNodeT<CYapfRouteLinkNodeKeyT, CYapfRouteLinkNodeT> Base;

	const LinkGraph *m_lg; ///< Link graph the edges belong to.

	/** Initialize this node. */
	inline void Set(CYapfRouteLinkNodeT *parent, ConstEdge *edge)
	{
		Base::Set(parent, false);
		this->m_key.Set(edge);
	}

	/** Get the edge of this node. */
	inline ConstEdge *GetEdge() const
	{
		return this->m_key.m_edge;
	}

	/** Get destination station. */
	inline const Station *GetDestination() const
	{
		return Station::Get((*this->m_lg)[this->GetEdge()->dest_node].station);
	}
};

typedef CNodeList_HashTableT<CYapfRouteLinkNodeT, 8, 10, 2048> CRouteLinkNodeList;

/** Link graph follower. */
struct CFollowLinkEdgeT {
	using EdgeList = span<ConstEdge>;

	const LinkGraph *m_lg; ///< Link graph the edges belong to.

	ConstEdge *m_from;     ///< Incoming edge.
	EdgeList m_to;         ///< Outgoing edges.

	CFollowLinkEdgeT(const LinkGraph *lg) : m_lg(lg), m_from(nullptr) {}

	/** Fill in edges reachable by this edge. */
	inline bool Follow(ConstEdge *from)
	{
		this->m_from = from;
		this->m_to = EdgeList{ (*m_lg)[from->dest_node].edges };

		return !this->m_to.empty();
	}
};

/** YAPF cost provider for cargo routing. */
template <class Types>
class CYapfCostRouteLinkT {
	typedef typename Types::Tpf Tpf;                     ///< The pathfinder class (derived from THIS class).
	typedef typename Types::TrackFollower Follower;      ///< The route follower.
	typedef typename Types::NodeList::Titem Node;        ///< This will be our node type.

	/** To access inherited path finder. */
	inline Tpf &Yapf()
	{
		return *static_cast<Tpf *>(this);
	}

	/** Cost of a single route link. */
	inline int EdgeCost(const Node &from, const Node &to) const
	{
		return DistanceManhattan(from.GetDestination()->xy, to.GetDestination()->xy);
	}

public:
	/** Called by YAPF to calculate the cost from the origin to the given node. */
	inline bool PfCalcCost(Node &n, const Follower *follow)
	{
		int segment_cost = EdgeCost(*n.m_parent, n);

		/* Apply it. */
		n.m_cost = n.m_parent->m_cost + segment_cost;
		return true;
	}
};

/** YAPF origin provider for cargo routing. */
template <class Types>
class CYapfOriginRouteLinkT {
	typedef typename Types::Tpf Tpf;                     ///< The pathfinder class (derived from THIS class).
	typedef typename Types::NodeList::Titem Node;        ///< This will be our node type.

	CargoID                m_cid;
	TileIndex              m_src;
	std::vector<std::pair<LinkGraphID, ConstEdge>> m_origin;

	/** To access inherited path finder. */
	inline Tpf &Yapf()
	{
		return *static_cast<Tpf *>(this);
	}

public:
	/** Get the current cargo type. */
	inline CargoID GetCargoID() const
	{
		return this->m_cid;
	}

	/** Set origin. */
	void SetOrigin(CargoID cid, TileIndex src, const StationList *stations)
	{
		this->m_cid = cid;
		this->m_src = src;

		/* Create fake edges for the starting nodes. */
		for (const Station *st : *stations) {
			if (LinkGraph::IsValidID(st->goods[cid].link_graph)) {
				m_origin.emplace_back(st->goods[cid].link_graph, ConstEdge(st->goods[cid].node));
			}
		}
	}

	/** Called when YAPF needs to place origin nodes into the open list. */
	void PfSetStartupNodes()
	{
		for (auto &o : m_origin) {
			Node &n = this->Yapf().CreateNewNode();
			n.m_lg = LinkGraph::Get(o.first);
			n.Set(nullptr, &o.second);

			this->Yapf().AddStartupNode(n);
		}
	}
};

/** YAPF destination provider for route links. */
template <class Types>
class CYapfDestinationRouteLinkT {
	typedef typename Types::Tpf Tpf;                     ///< The pathfinder class (derived from THIS class).
	typedef typename Types::NodeList::Titem Node;        ///< This will be our node type.

	TileArea m_dest;
	std::vector<std::pair<LinkGraphID, NodeID>> m_dest_st;

	/** To access inherited path finder. */
	inline Tpf &Yapf()
	{
		return *static_cast<Tpf *>(this);
	}

public:
	/** Set destination. */
	bool SetDestination(CargoID cid, const TileArea &dest, const StationList *stations)
	{
		this->m_dest = dest;

		for (const Station *st : *stations) {
			if (LinkGraph::IsValidID(st->goods[cid].link_graph)) {
				this->m_dest_st.emplace_back(st->goods[cid].link_graph, st->goods[cid].node);
			}
		}

		return !this->m_dest_st.empty();
	}

	/** Called by YAPF to detect if the node reaches the destination. */
	inline bool PfDetectDestination(const Node &n) const
	{
		return std::find_if(this->m_dest_st.begin(), this->m_dest_st.end(), [&n](const auto &d) { return n.m_lg->index == d.first && n.GetEdge()->dest_node == d.second; }) != this->m_dest_st.end();
	}

	/** Called by YAPF to calculate the estimated cost to the destination. */
	inline bool PfCalcEstimate(Node &n)
	{
		if (this->PfDetectDestination(n)) {
			n.m_estimate = n.m_cost;
			return true;
		}

		/* Estimate based on Manhattan distance to destination. */
		int d = DistanceManhattan(n.GetDestination()->xy, this->m_dest.tile);

		n.m_estimate = n.m_cost + d;
		assert(n.m_estimate >= n.m_parent->m_estimate);
		return true;
	}
};

/** Main route finding class. */
template <class Types>
class CYapfFollowLinkGraphT {
	typedef typename Types::Tpf Tpf;                     ///< The pathfinder class (derived from THIS class).
	typedef typename Types::TrackFollower Follower;      ///< The route follower.
	typedef typename Types::NodeList::Titem Node;        ///< This will be our node type.

	/** To access inherited path finder. */
	inline Tpf &Yapf()
	{
		return *static_cast<Tpf *>(this);
	}

public:
	/** Called by YAPF to move from the given node to the next nodes. */
	inline void PfFollowNode(Node &old_node)
	{
		Follower f(old_node.m_lg);

		if (f.Follow(old_node.GetEdge())) {
			for (auto &e : f.m_to) {
				/* Add new node. */
				Node &n = this->Yapf().CreateNewNode();
				n.m_lg = old_node.m_lg;
				n.Set(&old_node, &e);
				this->Yapf().AddNewNode(n, f);
			}
		}
	}

	/** Return debug report character to identify the transportation type. */
	inline char TransportTypeChar() const
	{
		return 'c';
	}

	/** Find the best cargo routing from a station to a destination. */
	static std::pair<StationID, OrderID> ChooseCargoRoute(CargoID cid, const StationList *src_stations, TileIndex src, const TileArea &dest)
	{
		/* Find possible destination stations. */
		StationFinder dest_stations(dest);

		/* Initialize pathfinder instance. */
		Tpf pf;
		pf.SetOrigin(cid, src, src_stations);
		if (!pf.SetDestination(cid, dest, dest_stations.GetStations())) return { INVALID_STATION, INVALID_ORDER };

		/* Do it. Exit if we didn't find a path. */
		if (!pf.FindPath(nullptr)) return { INVALID_STATION, INVALID_ORDER };

		/* Walk back to find the start node. */
		Node *node = pf.GetBestNode();
		while (node->m_parent->m_parent != nullptr) {
			node = node->m_parent;
		}

		/* Return result. */
		StationID st_id = node->m_parent->GetDestination()->index;
		if (pf.GetBestNode()->m_parent->GetDestination()->index == st_id) {
			/* Path starts and ends at the same station, do local delivery. */
			return { st_id, INVALID_ORDER };
		}

		return { st_id, node->m_parent->GetEdge()->dest_order };
	}
};

/**
 * Config struct of YAPF for cargo routing.
 *  Defines all 6 base YAPF modules as classes providing services for CYapfBaseT.
 */
template <class Tpf_>
struct CYapfLinkGraph_TypesT {
	/** Types - shortcut for this struct type */
	typedef CYapfLinkGraph_TypesT<Tpf_>  Types;

	typedef Tpf_               Tpf;           ///< Pathfinder type
	typedef CFollowLinkEdgeT   TrackFollower; ///< Node follower
	typedef CRouteLinkNodeList NodeList;      ///< Node list type
	typedef Vehicle            VehicleType;   ///< Dummy type

	typedef CYapfBaseT<Types>                 PfBase;        ///< Base pathfinder class
	typedef CYapfFollowLinkGraphT<Types>      PfFollow;      ///< Node follower
	typedef CYapfOriginRouteLinkT<Types>      PfOrigin;      ///< Origin provider
	typedef CYapfDestinationRouteLinkT<Types> PfDestination; ///< Destination/distance provider
	typedef CYapfSegmentCostCacheNoneT<Types> PfCache;       ///< Cost cache provider
	typedef CYapfCostRouteLinkT<Types>        PfCost;        ///< Cost provider
};

struct CYapfLinkGraph : CYapfT<CYapfLinkGraph_TypesT<CYapfLinkGraph>> {};

/**
 * Find the best cargo routing from a station to a destination.
 *
 * @param cid      Cargo type to route.
 * @param stations Set of possible originating stations.
 * @param src      Source tile where the cargo is generated at or INVALID_TILE if already in flight.
 * @param dest    Destination tile area.
 * @return Pair containing the station and order ID of the best route to the destination or (INVALID_STATION, INVALID_ORDER) if no route was found.
 */
std::pair<StationID, OrderID> YapfChooseCargoRoute(CargoID cid, const StationList *stations, TileIndex src, const TileArea &dest)
{
	return CYapfLinkGraph::ChooseCargoRoute(cid, stations, src, dest);
}
