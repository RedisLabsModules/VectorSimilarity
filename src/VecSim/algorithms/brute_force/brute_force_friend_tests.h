
#include "VecSim/friend_test_decl.h"
// Allow the following tests to access the index private members.
INDEX_TEST_FRIEND_CLASS(BruteForceTest_brute_force_vector_update_test_Test)
INDEX_TEST_FRIEND_CLASS(BruteForceTest_resize_and_align_index_Test)
INDEX_TEST_FRIEND_CLASS(BruteForceTest_resize_and_align_index_largeInitialCapacity_Test)
INDEX_TEST_FRIEND_CLASS(BruteForceTest_brute_force_empty_index_Test)
INDEX_TEST_FRIEND_CLASS(BruteForceTest_brute_force_reindexing_same_vector_Test)
INDEX_TEST_FRIEND_CLASS(BruteForceTest_brute_force_reindexing_same_vector_different_id_Test)
INDEX_TEST_FRIEND_CLASS(BruteForceTest_test_delete_swap_block_Test)
INDEX_TEST_FRIEND_CLASS(BruteForceTest_test_dynamic_bf_info_iterator_Test)
INDEX_TEST_FRIEND_CLASS(BruteForceTest_brute_force_zero_minimal_capacity_Test)
INDEX_TEST_FRIEND_CLASS(BruteForceTest_preferAdHocOptimization_Test)
friend class BM_VecSimBasics_DeleteVectorBF_Benchmark;
