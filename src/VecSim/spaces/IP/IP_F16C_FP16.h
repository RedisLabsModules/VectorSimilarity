/*
 *Copyright Redis Ltd. 2021 - present
 *Licensed under your choice of the Redis Source Available License 2.0 (RSALv2) or
 *the Server Side Public License v1 (SSPLv1).
 */

#include <cstdint>
#include "VecSim/spaces/space_includes.h"
#include "VecSim/spaces/AVX_utils.h"

static void InnerProductStep(uint16_t *&pVect1, uint16_t *&pVect2, __m256 &sum) {
    // Convert 8 half-floats into floats and store them in 256 bits register.
    auto v1 = _mm256_cvtph_ps(_mm_loadu_si128((__m128i_u const *)(pVect1)));
    auto v2 = _mm256_cvtph_ps(_mm_loadu_si128((__m128i_u const *)(pVect2)));

    // sum = v1 * v2 + sum
    sum = _mm256_fmadd_ps(v1, v2, sum);
    pVect1 += 8;
    pVect2 += 8;
}

template <unsigned short residual> // 0..31
float FP16_InnerProductSIMD16_F16C(const void *pVect1v, const void *pVect2v, size_t dimension) {
    auto *pVect1 = (uint16_t *)pVect1v;
    auto *pVect2 = (uint16_t *)pVect2v;

    const uint16_t *pEnd1 = pVect1 + dimension;

    auto sum = _mm256_setzero_ps();

    if (residual) {
        // Deal with remainder first. `dim` is more than 32, so we have at least one block of 32
        // 16-bit float so mask loading is guaranteed to be safe.
        __mmask16 constexpr residuals_mask = (1 << (residual % 8)) - 1;
        // Convert the first 8 half-floats into floats and store them 256 bits register,
        // where the floats in the positions corresponding to residuals are zeros.
        auto v1 = _mm256_blend_ps(_mm256_setzero_ps(),
                                  _mm256_cvtph_ps(_mm_loadu_si128((__m128i_u const *)pVect1)),
                                  residuals_mask);
        auto v2 = _mm256_blend_ps(_mm256_setzero_ps(),
                                  _mm256_cvtph_ps(_mm_loadu_si128((__m128i_u const *)pVect2)),
                                  residuals_mask);
        sum = _mm256_mul_ps(v1, v2);
        pVect1 += residual % 8;
        pVect2 += residual % 8;
    }

    // We dealt with the residual part. We are left with some multiple of 8 16-bit floats.
    // In every iteration we process 1 chunk of 128bit (8 FP16)
    do {
        InnerProductStep(pVect1, pVect2, sum);
    } while (pVect1 < pEnd1);

    return 1.0f - _mm256_reduce_add_ps(sum);
}
