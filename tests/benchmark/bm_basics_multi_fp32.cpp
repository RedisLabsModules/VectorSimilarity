#include "bm_basics.h"
#include "VecSim/algorithms/brute_force/brute_force_multi.h"
#include "VecSim/algorithms/hnsw/hnsw_multi.h"

/**************************************
  Basic tests for multi value index.
***************************************/

bool BM_VecSimGeneral::is_multi = true;

size_t BM_VecSimGeneral::n_queries = 10000;
size_t BM_VecSimGeneral::dim = 768;
size_t BM_VecSimGeneral::M = 64;
size_t BM_VecSimGeneral::EF_C = 512;
size_t BM_VecSimGeneral::n_vectors = 1000000;

const char *BM_VecSimGeneral::hnsw_index_file =
    "tests/benchmark/data/DBpedia-n1M-cosine-d768-M64-EFC512.hnsw_v1";
const char *BM_VecSimGeneral::test_queries_file =
    "tests/benchmark/data/DBpedia-test_vectors-n10k.raw";

DEFINE_DELETE_VECTOR(DeleteVector_BF_FP32, fp32_index_t, BruteForceIndex_Multi, float, float,
                     VecSimAlgo_BF)
DEFINE_DELETE_VECTOR(DeleteVector_HNSW_FP32, fp32_index_t, HNSWIndex_Multi, float, float,
                     VecSimAlgo_HNSWLIB)
#include "bm_basics_define_n_register_fp32.h"

BENCHMARK_MAIN();
