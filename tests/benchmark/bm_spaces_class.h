/*
 *Copyright Redis Ltd. 2021 - present
 *Licensed under your choice of the Redis Source Available License 2.0 (RSALv2) or
 *the Server Side Public License v1 (SSPLv1).
 */

#include <benchmark/benchmark.h>
#include <random>
#include <unistd.h>
#include "VecSim/utils/arr_cpp.h"
#include "VecSim/spaces/space_includes.h"
#include "VecSim/spaces/space_aux.h"

class BM_VecSimSpaces_FP32 : public benchmark::Fixture {
protected:
    std::mt19937 rng;
    size_t dim;
    float *v1, *v2;
    Arch_Optimization opt;

public:
    BM_VecSimSpaces_FP32();
    ~BM_VecSimSpaces_FP32() {}

    void SetUp(const ::benchmark::State &state);
    void TearDown(const ::benchmark::State &state);
};

class BM_VecSimSpaces_FP64 : public benchmark::Fixture {
protected:
    std::mt19937 rng;
    size_t dim;
    double *v1, *v2;
    Arch_Optimization opt;

public:
    BM_VecSimSpaces_FP64();
    ~BM_VecSimSpaces_FP64() {}

    void SetUp(const ::benchmark::State &state);
    void TearDown(const ::benchmark::State &state);
};
