#include "IP_AVX512.h"
#include "VecSim/spaces/space_includes.h"
#include "VecSim/spaces/IP/IP.h"

double FP64_InnerProductSIMD8Ext_AVX512_impl(const void *pVect1v, const void *pVect2v, size_t qty) {
    double *pVect1 = (double *)pVect1v;
    double *pVect2 = (double *)pVect2v;

    const double *pEnd1 = pVect1 + qty;

    __m512d sum512 = _mm512_set1_pd(0);

    // In each iteration we calculate 8 doubles = 512 bits.
    while (pVect1 < pEnd1) {

        __m512d v1 = _mm512_loadu_pd(pVect1);
        pVect1 += 8;
        __m512d v2 = _mm512_loadu_pd(pVect2);
        pVect2 += 8;
        sum512 = _mm512_add_pd(sum512, _mm512_mul_pd(v1, v2));
    }

    return _mm512_reduce_add_pd(sum512);
}

double FP64_InnerProductSIMD8Ext_AVX512(const void *pVect1, const void *pVect2, size_t qty) {
    return 1.0 - FP64_InnerProductSIMD8Ext_AVX512_impl(pVect1, pVect2, qty);
}

double FP64_InnerProductSIMD2Ext_AVX512_impl(const void *pVect1v, const void *pVect2v, size_t qty) {
    double *pVect1 = (double *)pVect1v;
    double *pVect2 = (double *)pVect2v;

    size_t qty8 = qty >> 3 << 3;

    const double *pEnd1 = pVect1 + qty8;
    const double *pEnd2 = pVect1 + qty;

    __m512d sum512 = _mm512_set1_pd(0);

    while (pVect1 < pEnd1) {

        __m512d v1 = _mm512_loadu_pd(pVect1);
        pVect1 += 8;
        __m512d v2 = _mm512_loadu_pd(pVect2);
        pVect2 += 8;
        sum512 = _mm512_add_pd(sum512, _mm512_mul_pd(v1, v2));
    }

    __m128d v1, v2;
    __m128d sum_prod = _mm512_extractf64x2_pd(sum512, 0) + _mm512_extractf64x2_pd(sum512, 1) +
                       _mm512_extractf64x2_pd(sum512, 2) + _mm512_extractf64x2_pd(sum512, 3);

    while (pVect1 < pEnd2) {
        v1 = _mm_loadu_pd(pVect1);
        pVect1 += 2;
        v2 = _mm_loadu_pd(pVect2);
        pVect2 += 2;
        sum_prod = _mm_add_pd(sum_prod, _mm_mul_pd(v1, v2));
    }

    double PORTABLE_ALIGN16 TmpRes[2];
    _mm_store_pd(TmpRes, sum_prod);
    return TmpRes[0] + TmpRes[1];
}

double FP64_InnerProductSIMD2Ext_AVX512_noDQ_impl(const void *pVect1v, const void *pVect2v,
                                                  size_t qty) {
    double *pVect1 = (double *)pVect1v;
    double *pVect2 = (double *)pVect2v;

    size_t qty8 = qty >> 3 << 3;

    const double *pEnd1 = pVect1 + qty8;
    const double *pEnd2 = pVect1 + qty;

    __m512d sum512 = _mm512_set1_pd(0);

    while (pVect1 < pEnd1) {

        __m512d v1 = _mm512_loadu_pd(pVect1);
        pVect1 += 8;
        __m512d v2 = _mm512_loadu_pd(pVect2);
        pVect2 += 8;
        sum512 = _mm512_add_pd(sum512, _mm512_mul_pd(v1, v2));
    }

    // Store the res for the first qty / 8 of the vectors.
    double resHead = _mm512_reduce_add_pd(sum512);
    pVect1 = pVect1 + qty8;
    pVect2 = pVect2 + qty8;

    __m128d v1, v2;
    __m128d sum_prod = _mm_set1_pd(0);
    while (pVect1 < pEnd2) {
        v1 = _mm_loadu_pd(pVect1);
        pVect1 += 2;
        v2 = _mm_loadu_pd(pVect2);
        pVect2 += 2;
        sum_prod = _mm_add_pd(sum_prod, _mm_mul_pd(v1, v2));
    }

    double PORTABLE_ALIGN16 TmpRes[2];
    _mm_store_pd(TmpRes, sum_prod);
    return TmpRes[0] + TmpRes[1] + resHead;
}

double FP64_InnerProductSIMD2Ext_AVX512(const void *pVect1, const void *pVect2, size_t qty) {
    return 1.0 - FP64_InnerProductSIMD2Ext_AVX512_impl(pVect1, pVect2, qty);
}

double FP64_InnerProductSIMD2Ext_AVX512_noDQ(const void *pVect1, const void *pVect2, size_t qty) {

    return 1.0 - FP64_InnerProductSIMD2Ext_AVX512_noDQ_impl(pVect1, pVect2, qty);
}

double FP64_InnerProductSIMD8ExtResiduals_AVX512(const void *pVect1v, const void *pVect2v,
                                                 size_t qty) {
    size_t qty8 = qty >> 3 << 3;
    double res = FP64_InnerProductSIMD8Ext_AVX512_impl(pVect1v, pVect2v, qty8);
    double *pVect1 = (double *)pVect1v + qty8;
    double *pVect2 = (double *)pVect2v + qty8;

    size_t qty_left = qty - qty8;
    double res_tail = FP64_InnerProduct_impl(pVect1, pVect2, qty_left);
    return 1.0 - (res + res_tail);
}

double FP64_InnerProductSIMD2ExtResiduals_AVX512(const void *pVect1v, const void *pVect2v,
                                                 size_t qty) {
    size_t qty2 = qty >> 1 << 1;
    double res = FP64_InnerProductSIMD2Ext_AVX512_impl(pVect1v, pVect2v, qty2);
    double *pVect1 = (double *)pVect1v + qty2;
    double *pVect2 = (double *)pVect2v + qty2;

    size_t qty_left = qty - qty2;
    double res_tail = FP64_InnerProduct_impl(pVect1, pVect2, qty_left);
    return 1.0 - (res + res_tail);
}

double FP64_InnerProductSIMD2ExtResiduals_AVX512_noDQ(const void *pVect1v, const void *pVect2v,
                                                      size_t qty) {
    size_t qty2 = qty >> 1 << 1;
    double res = FP64_InnerProductSIMD2Ext_AVX512_noDQ_impl(pVect1v, pVect2v, qty2);
    double *pVect1 = (double *)pVect1v + qty2;
    double *pVect2 = (double *)pVect2v + qty2;

    size_t qty_left = qty - qty2;
    double res_tail = FP64_InnerProduct_impl(pVect1, pVect2, qty_left);
    return 1.0 - (res + res_tail);
}
