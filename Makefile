
ifeq (n,$(findstring n,$(firstword -$(MAKEFLAGS))))
DRY_RUN:=1
else
DRY_RUN:=
endif

ifneq ($(VG),)
VALGRIND=$(VG)
endif

ifeq ($(VALGRIND),1)
override DEBUG ?= 1
endif

ifneq ($(SAN),)
override DEBUG ?= 1
export ASAN_OPTIONS=detect_odr_violation=0

ifeq ($(SAN),mem)
override SAN=memory
else ifeq ($(SAN),addr)
override SAN=address
endif

ifeq ($(SAN),memory)
CMAKE_SAN=-DUSE_MSAN=ON
override CTEST_ARGS += --exclude-regex BruteForceTest.sanity_rinsert_1280

else ifeq ($(SAN),address)
CMAKE_SAN=-DUSE_ASAN=ON
else ifeq ($(SAN),leak)
else ifeq ($(SAN),thread)
else
$(error SAN=mem|addr|leak|thread)
endif

export SAN
endif # SAN

#----------------------------------------------------------------------------------------------

ROOT=.
MK.pyver:=3

ifeq ($(wildcard $(ROOT)/deps/readies/mk),)
$(shell mkdir -p deps; cd deps; git clone https://github.com/RedisLabsModules/readies.git)
endif
include $(ROOT)/deps/readies/mk/main

#----------------------------------------------------------------------------------------------

define HELPTEXT
make build
  DEBUG=1          # build debug variant
  VERBOSE=1        # print detailed build info
  VG|VALGRIND=1    # build for Valgrind
  SAN=type         # build with LLVM sanitizer (type=address|memory|leak|thread)
  SLOW=1           # don't run build in parallel (for diagnostics)
make pybind        # build Python bindings
make clean         # remove binary files
  ALL=1            # remove binary directories

make all           # build all libraries and packages

make unit_test     # run unit tests
  CTEST_ARGS=args    # extra CTest arguments
  VG|VALGRIND=1      # run tests with valgrind
make flow_test     # run flow tests
  TEST=file::name    # run specific test
  BB=1               # run with debugger, stop on BB()
make mem_test      # run memory tests
make benchmark	   # run benchmarks
make toxenv        # enter Tox environment (for debugging flow tests)

make platform      # build for specific Linux distribution
  OSNICK=nick        # Linux distribution to build for
  REDIS_VER=ver      # use Redis version `ver`
  TEST=1             # test aftar build
  PUBLISH=1          # publish (i.e. docker push) after build

make format          # fix formatting of sources
make check-format    # check formatting of sources

make sanbox        # create container with CLang Sanitizer

endef

#----------------------------------------------------------------------------------------------

BINDIR=$(BINROOT)
TARGET=$(BINDIR)/libVectorSimilarity.so
SRCDIR=src

MK_CUSTOM_CLEAN=1

ifeq ($(SLOW),1)
MAKE_J=
else
MAKE_J:=-j$(shell nproc)
endif

#----------------------------------------------------------------------------------------------

ifeq ($(ARCH),x64)

ifeq ($(SAN),)
ifneq ($(findstring centos,$(OSNICK)),)
VECSIM_ARCH ?= skylake-avx512
else
VECSIM_ARCH ?= x86-64-v4
endif
else
VECSIM_ARCH ?= skylake-avx512
endif

CMAKE_VECSIM=-DVECSIM_ARCH=$(VECSIM_ARCH)

else # ARCH != x64

CMAKE_VECSIM=

endif # ARCH

#----------------------------------------------------------------------------------------------

CMAKE_DIR=$(ROOT)/src

CMAKE_FILES= \
	src/CMakeLists.txt \
	src/VecSim/spaces/CMakeLists.txt \
	cmake/common.cmake \
	cmake/gtest.cmake \
	cmake/clang-sanitizers.cmake

ifeq ($(DEBUG),1)
CMAKE_BUILD_TYPE=DEBUG
else
CMAKE_BUILD_TYPE=RelWithDebInfo
endif
CMAKE_FLAGS += -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)

ifeq ($(VERBOSE),1)
CMAKE_FLAGS += -DCMAKE_VERBOSE_MAKEFILE=on
endif

CMAKE_FLAGS += \
	-Wno-deprecated \
	-DCMAKE_WARN_DEPRECATED=OFF \
	-Wno-dev \
	--no-warn-unused-cli \
	-DOSNICK=$(OSNICK) \
	-DARCH=$(ARCH) \
	$(CMAKE_SAN) \
	$(CMAKE_VECSIM)

#----------------------------------------------------------------------------------------------

include $(MK)/defs

#----------------------------------------------------------------------------------------------

all: bindirs $(TARGET)

.PHONY: all

include $(MK)/rules

#----------------------------------------------------------------------------------------------

.PHONY: __force

$(BINDIR)/Makefile: __force
	$(SHOW)cd $(BINDIR) && cmake $(CMAKE_FLAGS) $(CMAKE_DIR)

$(TARGET): $(BINDIR)/Makefile
	@echo Building $(TARGET) ...
ifneq ($(DRY_RUN),1)
	$(SHOW)$(MAKE) -C $(BINDIR) $(MAKE_J)
else
	@make -C $(BINDIR) $(MAKE_J)
endif

clean:
ifeq ($(ALL),1)
	$(SHOW)rm -rf $(BINROOT)
else
	$(SHOW)$(MAKE) -C $(BINDIR) clean
endif

.PHONY: clean

#----------------------------------------------------------------------------------------------

pybind:
	$(SHOW)python3 -m poetry build

#----------------------------------------------------------------------------------------------

_CTEST_ARGS=$(CTEST_ARGS)
_CTEST_ARGS += \
	--output-on-failure \
	$(MAKE_J)

ifeq ($(VERBOSE),1)
_CTEST_ARGS += -V
endif

ifeq ($(VALGRIND),1)
_CTEST_ARGS += \
	-T memcheck \
	--overwrite MemoryCheckCommandOptions="--leak-check=full"
endif

unit_test:
	$(SHOW)cd $(BINDIR)/unit_tests && GTEST_COLOR=1 ctest $(_CTEST_ARGS)

flow_test:
ifneq ($(VIRTUAL_ENV),)
	$(SHOW)cd tests/flow && python3 -m pytest $(TEST)
else
	$(SHOW)python3 -m tox -e flowenv
endif

mem_test:
	$(SHOW)python3 -m RLTest --test $(ROOT)/tests/module/flow/test_vecsim_malloc.py \
		--module $(BINDIR)/module_tests//memory_test.so

benchmark:
	$(SHOW)$(BINDIR)/benchmark/bf_benchmark

toxenv:
ifeq ($(wildcard .tox,)
	$(SHOW)tox -e flowenv
endif
	$(SHOW)bash -c ". ./.tox/flowenv/bin/activate; $$SHELL"

.PHONY: unit_test flow_test mem_test benchmark toxenv

#----------------------------------------------------------------------------------------------

check-format:
	$(SHOW)./sbin/check-format.sh

format:
	$(SHOW)FIX=1 ./sbin/check-format.sh

.PHONY: check-format format

#----------------------------------------------------------------------------------------------

platform:
	$(SHOW)make -C build/platforms build PACK=1 ARTIFACTS=1
ifeq ($(PUBLISH),1)
	$(SHOW)make -C build/platforms publish
endif

.PHONY: platform

#----------------------------------------------------------------------------------------------

ifneq ($(wildcard /w/*),)
SANBOX_ARGS += -v /w:/w
endif

sanbox:
	@docker run -it -v $(PWD):/vecsim -w /vecsim --cap-add=SYS_PTRACE --security-opt seccomp=unconfined $(SANBOX_ARGS) redisfab/clang:13-x64-bullseye bash

.PHONY: sanbox
