#pragma once

#include <stdlib.h>
#include "VecSim/vec_sim_common.h"
#include <VecSim/query_results.h>
#include <utility>

template <typename dist_t>
struct CompareByFirst {
    constexpr bool operator()(std::pair<dist_t, unsigned int> const &a,
                              std::pair<dist_t, unsigned int> const &b) const noexcept {
        return (a.first != b.first) ? a.first < b.first : a.second < b.second;
    }
};

void float_vector_normalize(float *x, size_t dim);

void sort_results_by_id(VecSimQueryResult_List results);

void sort_results_by_score(VecSimQueryResult_List results);

const char *VecSimType_ToString(VecSimType vecsimType);

const char *VecSimMetric_ToString(VecSimMetric vecsimMetric);
