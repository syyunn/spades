//***************************************************************************
//* Copyright (c) 2018 Saint Petersburg State University
//* All Rights Reserved
//* See file LICENSE for details.
//***************************************************************************

#pragma once

#include "io/id_mapper.hpp"
#include "io_base.hpp"

#include "assembly_graph/core/graph.hpp"
#include "common/sequence/sequence.hpp"

namespace io {

namespace binary {

template<typename Graph>
class GraphIO : public IOSingle<Graph> {
public:
    GraphIO()
            : IOSingle<Graph>("debruijn graph", ".grseq") {
    }

    const IdMapper<typename Graph::EdgeId> &GetEdgeMapper() {
        return edge_mapper_;
    }

    void Write(BinOStream &str, const Graph &graph) override {
        str << graph.GetGraphIdDistributor().GetMax();

        for (auto v1 : graph) {
            str << v1.int_id() << graph.conjugate(v1).int_id();
            for (auto e1 : graph.OutgoingEdges(v1)) {
                auto e2 = graph.conjugate(e1);
                if (e2 < e1)
                    continue;
                str << e1.int_id() << e2.int_id()
                    << graph.EdgeEnd(e1).int_id() << graph.EdgeStart(e2).int_id()
                    << graph.EdgeNucls(e1);
            }
            str << (size_t)0; //null-term
        }
    }

    void Read(BinIStream &str, Graph &graph) override {
        graph.clear();
        size_t max_id;
        str >> max_id;
        auto id_storage = graph.GetGraphIdDistributor().Reserve(max_id, /*force_zero_shift*/true);

        auto TryAddVertex = [&](size_t ids[2]) {
            if (vertex_mapper_.count(ids[0]))
                return;
            TRACE("Vertex " << ids[0] << " ~ " << ids[1] << " .");
            auto id_distributor = id_storage.GetSegmentIdDistributor(ids, ids + 2);
            auto new_id = graph.AddVertex(typename Graph::VertexData(), id_distributor);
            vertex_mapper_[ids[0]] = new_id;
            vertex_mapper_[ids[1]] = graph.conjugate(new_id);
        };

        while (str.has_data()) { //Read until the end
            // FIXME use two separate ids instead of C-array! C-array are error-prone and could be easily mixed up with pointers!
            size_t start_ids[2];
            str >> start_ids;
            TryAddVertex(start_ids);
            while (true) {
                size_t edge_ids[2];
                str >> edge_ids[0];
                if (!edge_ids[0]) //null-term
                    break;
                str >> edge_ids[1];
                size_t end_ids[2];
                Sequence seq;
                str >> end_ids >> seq;
                TRACE("Edge " << edge_ids[0] << " : " << start_ids[0] << " -> "
                              << end_ids[0] << " l = " << seq.size() << " ~ " << edge_ids[1]);
                TryAddVertex(end_ids);

                VERIFY_MSG(!edge_mapper_.count(edge_ids[0]), edge_ids[0] << " is not unique");
                auto id_distributor = id_storage.GetSegmentIdDistributor(edge_ids, edge_ids + 2);
                auto new_id = graph.AddEdge(vertex_mapper_[start_ids[0]], vertex_mapper_[end_ids[0]], seq, id_distributor);
                edge_mapper_[edge_ids[0]] = new_id;
                edge_mapper_[edge_ids[1]] = graph.conjugate(new_id);
            }
        }
    }

private:
    IdMapper<typename Graph::VertexId> vertex_mapper_;
    IdMapper<typename Graph::EdgeId> edge_mapper_;

    DECL_LOGGER("GraphIO");
};

template<>
struct IOTraits<debruijn_graph::Graph> {
    typedef GraphIO<debruijn_graph::Graph> Type;
};

} // namespace binary

} // namespace io