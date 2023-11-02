/*
 *Copyright Redis Ltd. 2021 - present
 *Licensed under your choice of the Redis Source Available License 2.0 (RSALv2) or
 *the Server Side Public License v1 (SSPLv1).
 */

#include "IP.h"
#include <cstring>

float BFP16_InnerProduct(const void *pVect1, const void *pVect2, size_t dimension) {
    float *vec1 = (float *)pVect1;
    float *vec2 = (float *)pVect2;

    float res = 0;
    for (size_t i = 0; i < dimension; i++) {
        float a = 0;
        float b = 0;
        memcpy((char *)&a + 2, (char *)(vec1 + i) + 2, sizeof(u_int16_t));
        memcpy((char *)&b + 2, (char *)(vec2 + i) + 2, sizeof(u_int16_t));
        res += a * b;
    }
    return 1.0f - res;
}

float FP32_InnerProduct(const void *pVect1, const void *pVect2, size_t dimension) {
    float *vec1 = (float *)pVect1;
    float *vec2 = (float *)pVect2;

    float res = 0;
    for (size_t i = 0; i < dimension; i++) {
        res += vec1[i] * vec2[i];
    }
    return 1.0f - res;
}

double FP64_InnerProduct(const void *pVect1, const void *pVect2, size_t dimension) {
    double *vec1 = (double *)pVect1;
    double *vec2 = (double *)pVect2;

    double res = 0;
    for (size_t i = 0; i < dimension; i++) {
        res += vec1[i] * vec2[i];
    }
    return 1.0 - res;
}
