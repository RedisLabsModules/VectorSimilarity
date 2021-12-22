#pragma once

#include "VecSim/batch_iterator.h"
#include "hnswlib_c.h"

typedef size_t labelType;
typedef uint idType;

using namespace std;

class HNSW_BatchIterator : public VecSimBatchIterator {
private:
    const HNSWIndex *index;
    vecsim_stl::min_priority_queue<pair<float, labelType>>
        results;        // results to return immediately in the next iteration.
    idType entry_point; // internal id of the node to begin the scan from in the next iteration.
    bool allow_returned_candidates; // flag that indicates if we allow the search to visit in nodes
                                    // that where returned in previous iterations
    hnswlib::VisitedNodesHandler *visited_list; // Pointer to the hnsw visitedList structure.
    ushort tag_range_start; // save the minimal tag which is used to mark nodes that were visited
                            // and/or returned by this iterator.
    ushort cur_visited_tag; // used to mark nodes that were scanned in this iteration (that hasn't
                            // returned before by the iterator).
    ushort cur_returned_visited_tag; // use to mark nodes that were returned in previous iteration,
                                     // and scanned in the current iteration
    ushort iteration_num;
    short max_iterations; // The maximum number of iterations allowed before iterator will be
                          // depleted.
    bool depleted;
    float lower_bound;

    // experimental
    vecsim_stl::max_priority_queue<pair<float, idType>> top_candidates_extras;
    vecsim_stl::min_priority_queue<pair<float, idType>> candidates;

    vecsim_stl::max_priority_queue<pair<float, idType>> scanGraph();
    inline bool hasReturned(idType node_id) const;
    inline void markReturned(idType node_id);
    inline void unmarkReturned(idType node_id);
    inline void visitNode(idType node_id);
    inline bool hasVisitedInCurIteration(idType node_id) const;

public:
    HNSW_BatchIterator(const void *query_vector, const HNSWIndex *index,
                       std::shared_ptr<VecSimAllocator> allocator, short max_iterations = 500);

    VecSimQueryResult_List getNextResults(size_t n_res, VecSimQueryResult_Order order) override;

    bool isDepleted() override;

    void reset() override;

    ~HNSW_BatchIterator() override = default;
};
