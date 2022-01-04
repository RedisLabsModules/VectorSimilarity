#include "space_aux.h"

Arch_Optimization getArchitectureOptimization() {

#ifdef CPU_FEATURES_ARCH_X86
#include "cpuinfo_x86.h"
    cpu_features::X86Features features = cpu_features::GetX86Info().features;
    if (features.avx512f) {
        return ARCH_OPT_AVX512;
    } else if (features.avx || features.avx2) {
        return ARCH_OPT_AVX;
    } else if (features.sse || features.sse2 || features.sse3 || features.sse4_1 ||
               features.sse4_2 || features.sse4a) {
        return ARCH_OPT_SSE;
    }
#endif // CPU_FEATURES_ARCH_X86

    return ARCH_OPT_NONE;
}
