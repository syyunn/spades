//***************************************************************************
//* Copyright (c) 2015 Saint Petersburg State University
//* Copyright (c) 2011-2014 Saint Petersburg Academic University
//* All Rights Reserved
//* See file LICENSE for details.
//***************************************************************************

#ifndef PAIR_INFO_FILLER_HPP_
#define PAIR_INFO_FILLER_HPP_

#include "assembly_graph/graph_alignment/sequence_mapper_notifier.hpp"

namespace debruijn_graph {

/**
 * As for now it ignores sophisticated case of repeated consecutive
 * occurrence of edge in path due to gaps in mapping
 *
 * todo talk with Anton about simplification and speed-up of procedure with little quality loss
 */
class LatePairedIndexFiller : public SequenceMapperListener {
    typedef std::pair<EdgeId, EdgeId> EdgePair;
    typedef std::function<double(const EdgePair&, const MappingRange&, const MappingRange&)> WeightF;
public:
    LatePairedIndexFiller(const Graph &graph, WeightF weight_f, omnigraph::de::UnclusteredPairedInfoIndexT<Graph>& paired_index)
            : graph_(graph),
              weight_f_(std::move(weight_f)),
              paired_index_(paired_index),
              buffer_pi_(graph) {
    }

    void StartProcessLibrary(size_t) override {
        DEBUG("Start processing: start");
        buffer_pi_.clear();
        DEBUG("Start processing: end");
    }

    void StopProcessLibrary() override {
        paired_index_.Merge(buffer_pi_);
        buffer_pi_.clear();
    }
    
    void ProcessPairedRead(size_t,
                           const io::PairedRead& r,
                           const MappingPath<EdgeId>& read1,
                           const MappingPath<EdgeId>& read2) override {
        ProcessPairedRead(read1, read2, r.distance());
    }

    void ProcessPairedRead(size_t,
                           const io::PairedReadSeq& r,
                           const MappingPath<EdgeId>& read1,
                           const MappingPath<EdgeId>& read2) override {
        ProcessPairedRead(read1, read2, r.distance());
    }

    virtual ~LatePairedIndexFiller() {}

private:
    void ProcessPairedRead(const MappingPath<EdgeId>& path1,
                           const MappingPath<EdgeId>& path2, size_t read_distance) {
        for (size_t i = 0; i < path1.size(); ++i) {
            std::pair<EdgeId, MappingRange> mapping_edge_1 = path1[i];
            for (size_t j = 0; j < path2.size(); ++j) {
                std::pair<EdgeId, MappingRange> mapping_edge_2 = path2[j];

                EdgePair ep{mapping_edge_1.first, mapping_edge_2.first};

                omnigraph::de::DEWeight weight =
                        weight_f_(ep, mapping_edge_1.second, mapping_edge_2.second);
                size_t kmer_distance = read_distance
                        + mapping_edge_2.second.initial_range.end_pos
                        - mapping_edge_1.second.initial_range.start_pos;
                int edge_distance = (int) kmer_distance
                        + (int) mapping_edge_1.second.mapped_range.start_pos
                        - (int) mapping_edge_2.second.mapped_range.end_pos;

                if (math::gr(weight, 0)) {
                    buffer_pi_.Add(mapping_edge_1.first, mapping_edge_2.first,
                                   omnigraph::de::RawPoint(edge_distance, weight));

                }
            }
        }
    }

private:
    const Graph& graph_;
    WeightF weight_f_;
    omnigraph::de::UnclusteredPairedInfoIndexT<Graph>& paired_index_;
    omnigraph::de::ConcurrentPairedInfoBuffer<Graph> buffer_pi_;

    DECL_LOGGER("LatePairedIndexFiller");
};


}


#endif /* PAIR_INFO_FILLER_HPP_ */
