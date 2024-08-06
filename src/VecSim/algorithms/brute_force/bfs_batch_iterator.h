/*
 *Copyright Redis Ltd. 2021 - present
 *Licensed under your choice of the Redis Source Available License 2.0 (RSALv2) or
 *the Server Side Public License v1 (SSPLv1).
 */

#pragma once
#include "bf_batch_iterator.h"

#include <limits>

template <typename DataType, typename DistType>
class BFS_BatchIterator : public BF_BatchIterator<DataType, DistType> {
public:
    BFS_BatchIterator(void *query_vector, const BruteForceIndex<DataType, DistType> *index,
                      VecSimQueryParams *queryParams, std::shared_ptr<VecSimAllocator> allocator)
        : BF_BatchIterator<DataType, DistType>(query_vector, index, queryParams, allocator) {}

    ~BFS_BatchIterator() override = default;

private:
    VecSimQueryReply_Code calculateScores() override {
        this->index_label_count = this->index->indexLabelCount();
        this->scores.reserve(this->index_label_count);

        idType curr_id = 0;
        auto vectors_it = this->index->getVectorsIterator();
        auto DistFunc = this->index->getDistFunc();
        while (auto *vector = vectors_it->next()) {
            // Compute the scores for every vector and extend the scores array.
            if (VECSIM_TIMEOUT(this->getTimeoutCtx())) {
                return VecSim_QueryReply_TimedOut;
            }
            auto score = DistFunc(vector, this->getQueryBlob(), this->index->getDim());
            this->scores.emplace_back(score, this->index->getVectorLabel(curr_id));
            ++curr_id;
        }
        assert(curr_id == this->index->indexSize());
        return VecSim_QueryReply_OK;
    }
};
