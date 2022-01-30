#include "IP_space.h"
#include "space_aux.h"
#include "VecSim/spaces/IP/IP.h"
#include "VecSim/spaces/IP/IP_SSE.h"
#include "VecSim/spaces/IP/IP_AVX.h"
#include "VecSim/spaces/IP/IP_AVX512.h"

// clang-format off

InnerProductSpace::InnerProductSpace(size_t dim, std::shared_ptr<VecSimAllocator> allocator)
    : SpaceInterface(allocator) {
    fstdistfunc_ = InnerProduct;
#if defined(__x86_64__)
    Arch_Optimization arch_opt = getArchitectureOptimization();

#if defined(HAVE_MARCH_X86_64_V4)
    if (arch_opt == ARCH_OPT_AVX512) {
        if (dim % 16 == 0) {
            fstdistfunc_ = InnerProductSIMD16Ext_AVX512;
        } else {
            fstdistfunc_ = InnerProductSIMD16ExtResiduals_AVX512;
        }
    } else

#elif defined(HAVE_MARCH_X86_64_V3)
    if (arch_opt == ARCH_OPT_AVX) {
        if (dim % 16 == 0) {
            fstdistfunc_ = InnerProductSIMD16Ext_AVX;
        } else if (dim % 4 == 0) {
            fstdistfunc_ = InnerProductSIMD4Ext_AVX;
        } else if (dim > 16) {
            fstdistfunc_ = InnerProductSIMD16ExtResiduals_AVX;
        } else if (dim > 4) {
            fstdistfunc_ = InnerProductSIMD4ExtResiduals_AVX;
        }
    } else

#elif defined(HAVE_MARCH_X86_64_V2)
	if (arch_opt == ARCH_OPT_SSE) {
        if (dim % 16 == 0) {
            fstdistfunc_ = InnerProductSIMD16Ext_SSE;
        } else if (dim % 4 == 0) {
            fstdistfunc_ = InnerProductSIMD4Ext_SSE;
        } else if (dim > 16) {
            fstdistfunc_ = InnerProductSIMD16ExtResiduals_SSE;
        } else if (dim > 4) {
            fstdistfunc_ = InnerProductSIMD4ExtResiduals_SSE;
        }
    } else
#endif

	{ (void) arch_opt; }
#endif // __x86_64__

    dim_ = dim;
    data_size_ = dim * sizeof(float);
}

// clang-format on

size_t InnerProductSpace::get_data_size() const { return data_size_; }

DISTFUNC<float> InnerProductSpace::get_dist_func() const { return fstdistfunc_; }

void *InnerProductSpace::get_data_dim() { return &dim_; }

InnerProductSpace::~InnerProductSpace() = default;
