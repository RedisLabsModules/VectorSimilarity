/*
 *Copyright Redis Ltd. 2021 - present
 *Licensed under your choice of the Redis Source Available License 2.0 (RSALv2) or
 *the Server Side Public License v1 (SSPLv1).
 */

#include "AVX512BW_VBMI2.h"

#include "VecSim/spaces/L2/L2_AVX512F_BW_VL_VNNI_INT8.h"

namespace spaces {

#include "implementation_chooser.h"

dist_func_t<float> Choose_INT8_L2_implementation_AVX512F_BW_VL_VNNI(size_t dim) {
    dist_func_t<float> ret_dist_func;
    CHOOSE_IMPLEMENTATION(ret_dist_func, dim, 32, INT8_L2SqrSIMD32_AVX512F_BW_VL_VNNI);
    return ret_dist_func;
}

#include "implementation_chooser_cleanup.h"

} // namespace spaces
