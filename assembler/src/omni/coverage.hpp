/*
 * coverage.hpp
 *
 *  Created on: Jun 21, 2011
 *      Author: sergey
 */

#ifndef COVERAGE_HPP_
#define COVERAGE_HPP_

#include <tr1/unordered_map>
#include "logging.hpp"

namespace omnigraph {

template<class Graph>
class CoverageIndex: public GraphActionHandler<Graph> {
	typedef typename Graph::VertexId VertexId;
	typedef typename Graph::EdgeId EdgeId;
	typedef std::tr1::unordered_map<EdgeId, size_t> map_type;

private:

	Graph &g_;

	map_type storage_;

	size_t KPlusOneMerCoverage(EdgeId edge) const {
		return (size_t) (coverage(edge) * g_.length(edge));
	}

	template<class ReadThreader>
	void ProcessRead(const ReadThreader& threader, Read read) {
		Path<EdgeId> path = threader.MapSequence(
				Sequence(read.GetSequenceString()));
		if (path.sequence().size() == 0)
			return;
		const vector<EdgeId> &sequence = path.sequence();
		for (typename vector<EdgeId>::const_iterator it = sequence.begin(); it
				!= path.sequence().end(); ++it) {
			IncCoverage(*it, g_.length(*it));
		}
		IncCoverage(sequence[0], -path.start_pos());
		EdgeId last = sequence[sequence.size() - 1];
		IncCoverage(last, path.end_pos() - g_.length(last));
	}

public:
	CoverageIndex(Graph &g) :
		GraphActionHandler<Graph> ("CoverageIndex"), g_(g) {
		g_.AddActionHandler(this);
	}

	virtual ~CoverageIndex() {
		g_.RemoveActionHandler(this);
	}

	void SetCoverage(EdgeId edge, size_t cov) {
		storage_[edge] = cov;
	}

	/**
	 * Returns average coverage of the edge
	 */
	double coverage(EdgeId edge) const {
		auto it = storage_.find(edge);
		if (it == storage_.end()) {
			return 0;
		}
		return (double) it->second / g_.length(edge);
	}

	/**
	 * Method increases coverage value
	 */
	void IncCoverage(EdgeId edge, int toAdd) {
		storage_[edge] += toAdd;
	}

	/**
	 * Method increases coverage value by 1
	 */
	void IncCoverage(EdgeId edge) {
		IncCoverage(edge, 1);
	}

	template<class Stream, class ReadThreader>
	void FillIndex(Stream& stream, const ReadThreader& threader) {
		while (!stream.eof()) {
			Read read;
			stream >> read;
			ProcessRead(threader, read);
		}
	}

	virtual void HandleDelete(EdgeId edge) {
		storage_.erase(edge);
	}

	virtual void HandleMerge(vector<EdgeId> oldEdges, EdgeId newEdge) {
		size_t coverage = 0;
		for (typename vector<EdgeId>::iterator it = oldEdges.begin(); it
				!= oldEdges.end(); ++it) {
			coverage += KPlusOneMerCoverage(*it);
		}
		g_.SetCoverage(newEdge, coverage);
	}

	virtual void HandleGlue(EdgeId new_edge, EdgeId edge1, EdgeId edge2) {
		g_.IncCoverage(new_edge, KPlusOneMerCoverage(edge1));
		g_.IncCoverage(new_edge, KPlusOneMerCoverage(edge2));
	}

	virtual void HandleSplit(EdgeId oldEdge, EdgeId newEdge1, EdgeId newEdge2) {
		size_t length1 = g_.length(newEdge1);
		size_t length = g_.length(oldEdge);
		size_t coverage = KPlusOneMerCoverage(oldEdge);
		size_t coverage1 = coverage * length1 / length;
		if (coverage1 == 0)
			coverage1 = 1;
		size_t coverage2 = coverage - coverage1;
		if (coverage2 == 0)
			coverage2 = 1;
		g_.SetCoverage(newEdge1, coverage1);
		g_.SetCoverage(newEdge2, coverage2);
	}

};

}

#endif /* COVERAGE_HPP_ */
