/*
 *Copyright Redis Ltd. 2021 - present
 *Licensed under your choice of the Redis Source Available License 2.0 (RSALv2) or
 *the Server Side Public License v1 (SSPLv1).
 */

#pragma once
#include "VecSim/spaces/spaces.h"

namespace spaces {
dist_func_t<float> L2_FP32_GetDistFunc(size_t dim, unsigned char *alignment = nullptr,
                                       const void *arch_opt = nullptr);
dist_func_t<double> L2_FP64_GetDistFunc(size_t dim, unsigned char *alignment = nullptr,
                                        const void *arch_opt = nullptr);
dist_func_t<float> L2_BF16_GetDistFunc(size_t dim, unsigned char *alignment = nullptr,
                                       const void *arch_opt = nullptr);
dist_func_t<float> L2_FP16_GetDistFunc(size_t dim, unsigned char *alignment = nullptr,
                                       const void *arch_opt = nullptr);
} // namespace spaces
