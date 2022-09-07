#pragma once
#include "bf_batch_iterator.h"

#include <limits>

class BFM_BatchIterator : public BF_BatchIterator {
public:
    BFM_BatchIterator(void *query_vector, const BruteForceIndex<float, float> *index,
                      VecSimQueryParams *queryParams, std::shared_ptr<VecSimAllocator> allocator)
        : BF_BatchIterator(query_vector, index, queryParams, allocator) {}

    ~BFM_BatchIterator() override = default;

private:
    inline VecSimQueryResult_Code calculateScores() override {

        this->scores.reserve(this->index->indexLabelCount());
        // TODO: template temporary map
        vecsim_stl::unordered_map<labelType, float> tmp_scores(this->index->indexLabelCount(),
                                                               this->allocator);
        vecsim_stl::vector<VectorBlock *> blocks = this->index->getVectorBlocks();
        VecSimQueryResult_Code rc;

        idType curr_id = 0;
        for (auto &block : blocks) {
            // compute the scores for the vectors in every block and extend the scores array.
            auto block_scores = this->index->computeBlockScores(block, this->getQueryBlob(),
                                                                this->getTimeoutCtx(), &rc);
            if (VecSim_OK != rc) {
                return rc;
            }
            for (size_t i = 0; i < block_scores.size(); i++) {
                labelType curr_label = index->getVectorLabel(curr_id);
                auto curr_pair = tmp_scores.find(curr_label);
                // For each score, emplace or update the score of the label.
                if (curr_pair == tmp_scores.end()) {
                    tmp_scores.emplace(curr_label, block_scores[i]);
                } else if (curr_pair->second > block_scores[i]) {
                    curr_pair->second = block_scores[i];
                }
                ++curr_id;
            }
        }
        assert(curr_id == index->indexSize());
        for (auto p : tmp_scores) {
            this->scores.emplace_back(p.second, p.first);
        }
        return VecSim_QueryResult_OK;
    }
};
