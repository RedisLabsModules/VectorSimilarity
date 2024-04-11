/*
 *Copyright Redis Ltd. 2021 - present
 *Licensed under your choice of the Redis Source Available License 2.0 (RSALv2) or
 *the Server Side Public License v1 (SSPLv1).
 */
#define DATA_TYPE float
#include "bm_spaces.h"

// AVX512 functions
#ifdef OPT_AVX512F

INITIALIZE_BENCHMARKS_SET(FP32, AVX512_F, L2, 16);
#endif // AVX512F

// AVX functions
#ifdef OPT_AVX

INITIALIZE_BENCHMARKS_SET(FP32, AVX, L2, 16);
#endif // AVX

// SSE functions
#ifdef OPT_SSE

INITIALIZE_BENCHMARKS_SET(FP32, SSE, L2, 16);

#endif // SSE

INITIALIZE_NAIVE_BM(FP32, InnerProduct, 16);
INITIALIZE_NAIVE_BM(FP32, L2Sqr, 16);
BENCHMARK_MAIN();
