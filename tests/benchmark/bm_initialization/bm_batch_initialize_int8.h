#pragma once

/**************************************
  Define and register benchmarks for batch iterator with index of data type int8
  NOTE: benchmarks' tests order can affect their results. Please add new benchmarks at the end of
the file.
***************************************/

// Fixed size batch BF
BENCHMARK_TEMPLATE_DEFINE_F(BM_BatchIterator, BM_FUNC_NAME(BF, FixedBatchSize), int8_index_t)
(benchmark::State &st) { BF_FixedBatchSize(st); }
REGISTER_FixedBatchSize(BM_FUNC_NAME(BF, FixedBatchSize));

// Variable size batch BF
BENCHMARK_TEMPLATE_DEFINE_F(BM_BatchIterator, BM_FUNC_NAME(BF, VariableBatchSize), int8_index_t)
(benchmark::State &st) { BF_VariableBatchSize(st); }
REGISTER_VariableBatchSize(BM_FUNC_NAME(BF, VariableBatchSize));

// Batches to hadoc BF
BENCHMARK_TEMPLATE_DEFINE_F(BM_BatchIterator, BM_FUNC_NAME(BF, BatchesToAdhocBF), int8_index_t)
(benchmark::State &st) { BF_BatchesToAdhocBF(st); }
REGISTER_BatchesToAdhocBF(BM_FUNC_NAME(BF, BatchesToAdhocBF));

// Fixed size batch HNSW
BENCHMARK_TEMPLATE_DEFINE_F(BM_BatchIterator, BM_FUNC_NAME(HNSW, FixedBatchSize), int8_index_t)
(benchmark::State &st) { HNSW_FixedBatchSize(st); }
REGISTER_FixedBatchSize(BM_FUNC_NAME(HNSW, FixedBatchSize));

// Variable size batch BF
BENCHMARK_TEMPLATE_DEFINE_F(BM_BatchIterator, BM_FUNC_NAME(HNSW, VariableBatchSize), int8_index_t)
(benchmark::State &st) { HNSW_VariableBatchSize(st); }
REGISTER_VariableBatchSize(BM_FUNC_NAME(HNSW, VariableBatchSize));

// Batches to hadoc HSNW
BENCHMARK_TEMPLATE_DEFINE_F(BM_BatchIterator, BM_FUNC_NAME(HNSW, BatchesToAdhocBF), int8_index_t)
(benchmark::State &st) { HNSW_BatchesToAdhocBF(st); }

REGISTER_HNSW_BatchesToAdhocBF(BM_FUNC_NAME(HNSW, BatchesToAdhocBF));
