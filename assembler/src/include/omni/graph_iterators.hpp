#pragma once

#include "adt/queue_iterator.hpp"
#include "io/read_processor.hpp"
#include "action_handlers.hpp"

#include <boost/iterator/iterator_facade.hpp>

namespace omnigraph {

/**
 * SmartIterator is able to iterate through collection content of which can be changed in process of
 * iteration. And as GraphActionHandler SmartIterator can change collection contents with respect to the
 * way graph is changed. Also one can define order of iteration by specifying Comparator.
 */
template<class Graph, typename ElementId, typename Comparator = std::less<ElementId>>
class SmartIterator : public GraphActionHandler<Graph> {
    typedef GraphActionHandler<Graph> base;
    DynamicQueueIterator<ElementId, Comparator> inner_it_;
    bool add_new_;
    bool canonical_only_;

protected:

    void push(const ElementId& el) {
        if (!canonical_only_ || el <= this->g().conjugate(el)) {
            inner_it_.push(el);
        }
    }

    template<typename InputIterator>
    void insert(InputIterator begin, InputIterator end) {
        for (auto it = begin; it != end; ++it) {
            push(*it);
        }
    }

    void erase(const ElementId& el) {
        if (!canonical_only_ || el <= this->g().conjugate(el)) {
            inner_it_.erase(el);
        }
    }

    SmartIterator(const Graph &g, const string &name, bool add_new,
                  const Comparator& comparator, bool canonical_only)
            : base(g, name),
              inner_it_(comparator),
              add_new_(add_new),
              canonical_only_(canonical_only) {
    }

public:

    virtual ~SmartIterator() {
    }

    bool IsEnd() const {
        return inner_it_.IsEnd();
    }

    ElementId operator*() {
        return *inner_it_;
    }

    void operator++() {
        ++inner_it_;
    }

    void HandleAdd(ElementId v) override {
        if (add_new_)
            push(v);
    }

    void HandleDelete(ElementId v) override {
        erase(v);
    }

    //use carefully!
    void ReleaseCurrent() {
        inner_it_.ReleaseCurrent();
    }

};

/**
 * SmartIterator is abstract class which acts both as QueueIterator and GraphActionHandler. As QueueIterator
 * SmartIterator is able to iterate through collection content of which can be changed in process of
 * iteration. And as GraphActionHandler SmartIterator can change collection contents with respect to the
 * way graph is changed. Also one can define order of iteration by specifying Comparator.
 */
template<class Graph, typename ElementId,
         typename Comparator = std::less<ElementId>>
class SmartSetIterator : public SmartIterator<Graph, ElementId, Comparator> {
    typedef SmartIterator<Graph, ElementId, Comparator> base;

public:
    SmartSetIterator(const Graph &g,
                     const Comparator& comparator = Comparator(),
                     bool canonical_only = false)
            : base(g, "SmartSet " + ToString(this), false, comparator, canonical_only) {
    }

    template<class Iterator>
    SmartSetIterator(const Graph &g, Iterator begin, Iterator end,
                     const Comparator& comparator = Comparator(),
                     bool canonical_only = false)
            : SmartSetIterator(g, comparator, canonical_only) {
        insert(begin, end);
    }

    template<typename InputIterator>
    void insert(InputIterator begin, InputIterator end) {
        base::insert(begin, end);
    }

};

/*
 * ConditionedSmartSetIterator acts much like SmartSetIterator, but, unlike the above case,
 * one can (and must) provide merge handler that will decide whether to add merged edge to
 * the set being iterated or not (extending add_new_ parameter logic of SmartIterator)
 * Also has the ability to be `reset` (i.e. start from the begin-iterator with respect to
 * added and deleted values)
 * MergeHandler class/struct must provide:
 *  bool operator()(const std::vector<ElementId> &, ElementId)
 */
template<class Graph, typename ElementId, class MergeHandler>
class ConditionedSmartSetIterator : public SmartSetIterator<Graph, ElementId> {
    typedef SmartSetIterator<Graph, ElementId> base;

    MergeHandler &merge_handler_;
    std::unordered_set<ElementId> true_elements_;

  public:

    template <class Iterator>
    ConditionedSmartSetIterator(const Graph &graph, Iterator begin, Iterator end,
                                MergeHandler &merge_handler)
            : SmartSetIterator<Graph, ElementId>(graph, begin, end),
              merge_handler_(merge_handler),
              true_elements_() {

        for (auto it = begin; it != end; ++it) {
            true_elements_.insert(*it);
        }
    }

    void HandleAdd(ElementId v) override {
        TRACE("handleAdd " << this->g().str(v));
        if (true_elements_.count(v)) {
            this->push(v);
        }
    }

    void HandleDelete(ElementId v) override {
        TRACE("handleDel " << this->g().str(v));
        base::HandleDelete(v);
        true_elements_.erase(v);
    }

    void HandleMerge(const std::vector<ElementId>& old_edges, ElementId new_edge) override {
        TRACE("handleMer " << this->g().str(new_edge));
        if (merge_handler_(old_edges, new_edge)) {
            true_elements_.insert(new_edge);
        }
    }

private:
    DECL_LOGGER("ConditionedSmartSetIterator");
};

/**
 * SmartVertexIterator iterates through vertices of graph. It listens to AddVertex/DeleteVertex graph events
 * and correspondingly edits the set of vertices to iterate through. Note: high level event handlers are
 * triggered before low level event handlers like H>andleAdd/HandleDelete. Thus if Comparator uses certain
 * structure which is also updated with handlers make sure that all information is updated in high level
 * event handlers.
 */
template<class Graph, typename Comparator = std::less<typename Graph::VertexId> >
class SmartVertexIterator : public SmartIterator<Graph,
                                                 typename Graph::VertexId, Comparator> {
  public:
    typedef typename Graph::VertexId VertexId;

    static size_t get_id() {
        static size_t id = 0;
        return id++;
    }

  public:
    SmartVertexIterator(const Graph &g, const Comparator& comparator =
                        Comparator(), bool canonical_only = false)
            : SmartIterator<Graph, VertexId, Comparator>(
                g, "SmartVertexIterator " + ToString(get_id()), true,
                comparator, canonical_only) {
        this->insert(g.begin(), g.end());
    }

};

//todo return verifies when they can be switched off
template<class Graph>
class GraphEdgeIterator : public boost::iterator_facade<GraphEdgeIterator<Graph>
                                                    , typename Graph::EdgeId, boost::forward_traversal_tag
                                                    , typename Graph::EdgeId> {
    typedef typename Graph::EdgeId EdgeId;
    typedef typename Graph::VertexIt const_vertex_iterator;
    typedef typename Graph::edge_const_iterator const_edge_iterator;

    const Graph& g_;
    const_vertex_iterator v_it_;
    const_edge_iterator e_it_;
    bool canonical_only_;

public:

    GraphEdgeIterator(const Graph& g, const_vertex_iterator v_it, bool canonical_only = false)
            : g_(g),
              v_it_(v_it),
              canonical_only_(canonical_only) {
        if (v_it_ != g_.end()) {
            e_it_ = g_.out_begin(*v_it_);
            Skip();
        }
    }

private:
    friend class boost::iterator_core_access;

    void Skip() {
        //VERIFY(v_it_ != g_.end());
        while (e_it_ == g_.out_end(*v_it_)) {
            v_it_++;
            if (v_it_ == g_.end())
                return;
            e_it_ = g_.out_begin(*v_it_);
            if (canonical_only_) {
                EdgeId e = *e_it_;
                while (e_it_ != g_.out_end(*v_it_) && g_.conjugate(e) < e)
                    e_it_++;
            }
        }
    }

    void increment() {
        if (v_it_ == g_.end())
            return;
        e_it_++;
        Skip();
    }

    bool equal(const GraphEdgeIterator &other) const {
        if (other.v_it_ != v_it_)
            return false;
        if (other.canonical_only_ != canonical_only_)
            return false;
        if (v_it_ != g_.end() && other.e_it_ != e_it_)
            return false;
        return true;
    }

    EdgeId dereference() const {
        //VERIFY(v_it_ != g_.end());
        return *e_it_;
    }

};

template<class Graph>
class ConstEdgeIterator {
    typedef typename Graph::EdgeId EdgeId;
    GraphEdgeIterator<Graph> begin_, end_;

  public:
    ConstEdgeIterator(const Graph &g, bool canonical_only = false)
            : begin_(g, g.begin(), canonical_only), end_(g, g.end(), canonical_only) {
    }

    bool IsEnd() const {
        return begin_ == end_;
    }

    EdgeId operator*() const {
        return *begin_;
    }

    const ConstEdgeIterator& operator++() {
        begin_++;
        return *this;
    }
};

/**
 * SmartEdgeIterator iterates through edges of graph. It listens to AddEdge/DeleteEdge graph events
 * and correspondingly edits the set of edges to iterate through. Note: high level event handlers are
 * triggered before low level event handlers like HandleAdd/HandleDelete. Thus if Comparator uses certain
 * structure which is also updated with handlers make sure that all information is updated in high level
 * event handlers.
 */
template<class Graph, typename Comparator = std::less<typename Graph::EdgeId> >
class SmartEdgeIterator : public SmartIterator<Graph, typename Graph::EdgeId, Comparator> {
    typedef GraphEdgeIterator<Graph> EdgeIt;
  public:
    typedef typename Graph::EdgeId EdgeId;

    static size_t get_id() {
        static size_t id = 0;
        return id++;
    }

  public:
    //todo think of some parallel simplif problem O_o
    SmartEdgeIterator(const Graph &g, Comparator comparator = Comparator(),
                      bool canonical_only = false)
            : SmartIterator<Graph, EdgeId, Comparator>(
                g, "SmartEdgeIterator " + ToString(get_id()), true,
                comparator, canonical_only) {
        this->insert(EdgeIt(g, g.begin()), EdgeIt(g, g.end()));

//        for (auto it = graph.begin(); it != graph.end(); ++it) {
//            //todo: this solution doesn't work with parallel simplification
//            this->insert(graph.out_begin(*it), graph.out_end(*it));
//            //this does
//            //auto out = graph.OutgoingEdges(*it);
//            //this->base::insert(out.begin(), out.end());
//        }
    }
};

//todo move out
template<class Graph>
class ParallelEdgeProcessor {
    class ConstEdgeIteratorWrapper {
      public:
        typedef typename Graph::EdgeId ReadT;

        ConstEdgeIteratorWrapper(const Graph &g)
                : it_(g) {}

        bool eof() const { return it_.IsEnd(); }

        ConstEdgeIteratorWrapper& operator>>(typename Graph::EdgeId &val) {
            val = *it_;
            ++it_; 
            return *this;
        }

      private:
        ConstEdgeIterator<Graph> it_;
    };

  public:
    ParallelEdgeProcessor(const Graph &g, unsigned nthreads)
            : rp_(nthreads), it_(g) {}

    template <class Processor>
    bool Run(Processor &op) { return rp_.Run(it_, op); }

    bool IsEnd() const { return it_.eof(); }
    size_t processed() const { return rp_.processed(); }

  private:
    hammer::ReadProcessor rp_;
    ConstEdgeIteratorWrapper it_;
};

//todo move out
template<class Graph>
class ParallelIterationHelper {
    typedef typename Graph::EdgeId EdgeId;
    typedef typename Graph::VertexId VertexId;
    typedef typename Graph::VertexIt const_vertex_iterator;

    const Graph& g_;
public:

    ParallelIterationHelper(const Graph& g)
            : g_(g) {

    }

    std::vector<const_vertex_iterator> VertexChunks(size_t chunk_cnt) const {
        VERIFY(chunk_cnt > 0);
        //trying to split vertices into equal chunks, leftovers put into first chunk
        vector<const_vertex_iterator> answer;
        size_t vertex_cnt = g_.size();
        size_t chunk_size = vertex_cnt / chunk_cnt;
        auto it = g_.begin();
        answer.push_back(it);
        for (size_t i = 0; i + chunk_cnt * chunk_size < vertex_cnt; ++i) {
            it++;
        }
        if (chunk_size > 0) {
            size_t i = 0;
            do {
                ++it;
                if (++i % chunk_size == 0)
                    answer.push_back(it);
            } while (it != g_.end());

            VERIFY(i == chunk_cnt * chunk_size);
        } else {
            VERIFY(it == g_.end());
            answer.push_back(it);
        }
        VERIFY(answer.back() == g_.end());
        return answer;
    }

    std::vector<omnigraph::GraphEdgeIterator<Graph>> EdgeChunks(size_t chunk_cnt) const {
        vector<omnigraph::GraphEdgeIterator<Graph>> answer;
        for (const_vertex_iterator v_it : VertexChunks(chunk_cnt)) {
            answer.push_back(omnigraph::GraphEdgeIterator<Graph>(g_, v_it));
        }
        return answer;
    }
};

}
