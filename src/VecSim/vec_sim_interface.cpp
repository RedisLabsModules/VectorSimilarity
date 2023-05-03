/*
 *Copyright Redis Ltd. 2021 - present
 *Licensed under your choice of the Redis Source Available License 2.0 (RSALv2) or
 *the Server Side Public License v1 (SSPLv1).
 */

#include "VecSim/vec_sim_interface.h"
#include <cstdarg>
#include <iostream>

// Print log messages to stdout
void Vecsim_Log(void *ctx, const char *message) { std::cout << message << std::endl; }

timeoutCallbackFunction VecSimIndexInterface::timeoutCallback = [](void *ctx) { return 0; };
logCallbackFunction VecSimIndexInterface::logCallback = Vecsim_Log;

#ifdef BUILD_TESTS
static inline void Vecsim_Log_DO_NOTHING(void *ctx, const char *message) {}

void VecSimIndexInterface::resetLogCallbackFunction() {
    VecSimIndexInterface::logCallback = Vecsim_Log_DO_NOTHING;
}
#endif
