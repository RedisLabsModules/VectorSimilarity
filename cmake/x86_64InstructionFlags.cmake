# This file checks if the compiler supports certain CPU flags. If the flags are supported,
# it adds compilation definitions to include code sections that require these flags.

include(CheckCXXCompilerFlag)

if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "(x86_64)|(AMD64|amd64)|(^i.86$)")
	# build SSE/AVX* code only on x64 processors.
	# Check that the compiler supports instructions flag.
	# This will add the relevant flag both the the space selector and the optimization.
	CHECK_CXX_COMPILER_FLAG(-mavx512vl CXX_AVX512VL)
	CHECK_CXX_COMPILER_FLAG(-mavx512bf16 CXX_AVX512BF16)
	CHECK_CXX_COMPILER_FLAG(-mavx512bw CXX_AVX512BW)
	CHECK_CXX_COMPILER_FLAG(-mavx512vbmi2 CXX_AVX512VBMI2)
	CHECK_CXX_COMPILER_FLAG(-mavx512f CXX_AVX512F)
	CHECK_CXX_COMPILER_FLAG(-mavx2 CXX_AVX2)
	CHECK_CXX_COMPILER_FLAG(-mavx CXX_AVX)
	CHECK_CXX_COMPILER_FLAG(-mf16c CXX_F16C)
	CHECK_CXX_COMPILER_FLAG(-mfma CXX_FMA)
	CHECK_CXX_COMPILER_FLAG(-msse3 CXX_SSE3)
	CHECK_CXX_COMPILER_FLAG(-msse CXX_SSE)

	if(CXX_AVX512VL AND CXX_AVX512BF16)
		add_compile_definitions(OPT_AVX512_BF16_VL)
	endif()

	if(CXX_AVX512F)
		add_compile_definitions(OPT_AVX512F)
	endif()

	if(CXX_AVX512BW AND CXX_AVX512VBMI2)
		add_compile_definitions(OPT_AVX512_BW_VBMI2)
	endif()

	if(CXX_F16C AND CXX_FMA AND CXX_AVX)
		add_compile_definitions(OPT_F16C)
	endif()

	if(CXX_AVX2)
		add_compile_definitions(OPT_AVX2)
	endif()

	if(CXX_AVX)
		add_compile_definitions(OPT_AVX)
	endif()

	if(CXX_SSE3)
		add_compile_definitions(OPT_SSE3)
	endif()

	if(CXX_SSE)
		add_compile_definitions(OPT_SSE)
	endif()
endif()
