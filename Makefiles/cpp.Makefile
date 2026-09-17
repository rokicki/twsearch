TWSEARCH_VERSION=v0.0.0

.PHONY: build-cpp
build-cpp: build/bin/twsearch

# MAKEFLAGS += -j
# CXXFLAGS = -fsanitize=address -fsanitize=undefined -O3 -Warray-bounds -Wextra -Wall -pedantic -std=c++20 -g -Wsign-compare
CXXFLAGS = -O3 -Warray-bounds -Wextra -Wall -pedantic -std=c++20 -g -Wsign-compare
FLAGS = -DTWSEARCH_VERSION=${TWSEARCH_VERSION} -DUSE_PTHREADS -DUSE_PPQSORT
LDFLAGS = -lpthread

# TODO: why does this always trigger rebuilds when using as a target dependency?
CPP_MAKEFILE = Makefile/cpp.Makefile
${CPP_MAKEFILE}:

BASESOURCE = src/cpp/canon.cpp src/cpp/vendor/cityhash/src/city.cc \
   src/cpp/filtermoves.cpp src/cpp/generatingset.cpp src/cpp/index.cpp \
   src/cpp/parsemoves.cpp src/cpp/prunetable.cpp src/cpp/pruneio.cpp \
   src/cpp/puzdef.cpp src/cpp/readksolve.cpp src/cpp/rotations.cpp \
   src/cpp/solve.cpp src/cpp/threads.cpp src/cpp/twsearch.cpp src/cpp/util.cpp \
   src/cpp/workchunks.cpp src/cpp/cmds.cpp src/cpp/cmdlineops.cpp src/cpp/subgroup.cpp \
   src/cpp/cancel.cpp

EXTRASOURCE = src/cpp/antipode.cpp \
   src/cpp/coset.cpp src/cpp/descsets.cpp \
   src/cpp/findalgo.cpp src/cpp/god.cpp src/cpp/orderedgs.cpp \
   src/cpp/ordertree.cpp src/cpp/shorten.cpp src/cpp/unrotate.cpp \
   src/cpp/test.cpp src/cpp/totalvar.cpp src/cpp/beamsearch.cpp

CSOURCE = $(BASESOURCE) $(FFISOURCE) $(EXTRASOURCE)

OBJ = build/cpp/antipode.o build/cpp/canon.o build/cpp/cmdlineops.o \
   build/cpp/filtermoves.o build/cpp/findalgo.o build/cpp/generatingset.o build/cpp/god.o \
   build/cpp/index.o build/cpp/parsemoves.o build/cpp/prunetable.o build/cpp/pruneio.o build/cpp/puzdef.o \
   build/cpp/readksolve.o build/cpp/solve.o build/cpp/test.o build/cpp/threads.o \
   build/cpp/twsearch.o build/cpp/util.o build/cpp/workchunks.o build/cpp/rotations.o \
   build/cpp/orderedgs.o build/cpp/coset.o build/cpp/descsets.o \
   build/cpp/ordertree.o build/cpp/unrotate.o build/cpp/shorten.o \
   build/cpp/cmds.o build/cpp/beamsearch.o build/cpp/subgroup.o \
   build/cpp/totalvar.o build/cpp/cancel.o build/cpp/vendor/cityhash/city.o

HSOURCE = src/cpp/antipode.h src/cpp/canon.h src/cpp/cmdlineops.h \
   src/cpp/filtermoves.h src/cpp/findalgo.h src/cpp/generatingset.h src/cpp/god.h src/cpp/index.h \
   src/cpp/parsemoves.h src/cpp/prunetable.h src/cpp/puzdef.h src/cpp/readksolve.h src/cpp/solve.h \
   src/cpp/test.h src/cpp/threads.h src/cpp/util.h src/cpp/workchunks.h src/cpp/rotations.h \
   src/cpp/orderedgs.h src/cpp/twsearch.h src/cpp/coset.h src/cpp/descsets.h \
   src/cpp/ordertree.h src/cpp/unrotate.h src/cpp/shorten.h src/cpp/cmds.h \
   src/cpp/totalvar.h src/cpp/subgroup.h src/cpp/cancel.h

build/cpp:
	mkdir -p build/cpp

build/cpp/%.o: src/cpp/%.cpp Makefiles/cpp.Makefile $(HSOURCE) | build/cpp
	$(CXX) -I./src/cpp/vendor/cityhash/src -c $(CXXFLAGS) $(FLAGS) $< -o $@

build/cpp/vendor/cityhash:
	mkdir -p build/cpp/vendor/cityhash

build/cpp/vendor/cityhash/%.o: src/cpp/vendor/cityhash/src/%.cc Makefiles/cpp.Makefile | build/cpp/vendor/cityhash
	$(CXX) -I./src/cpp/vendor/cityhash/src -c $(CXXFLAGS) $(FLAGS) $< -o $@

build/bin/:
	mkdir -p build/bin/

build/bin/twsearch: $(OBJ) Makefiles/cpp.Makefile | build/bin/
	$(CXX) $(CXXFLAGS) -o build/bin/twsearch $(OBJ) $(LDFLAGS)

.PHONY: lint-cpp
lint-cpp:
	find ./src/cpp -iname "*.h" -o -iname "*.cpp" | grep -v ppqsort | xargs clang-format --dry-run -Werror

.PHONY: format-cpp
format-cpp:
	find ./src/cpp -iname "*.h" -o -iname "*.cpp" | grep -v ppqsort | xargs clang-format -i

.PHONY: cpp-clean
cpp-clean:
	rm -rf ./build

# C++ and `twsearch-cpp-wrapper` testing

.PHONY: test-cpp-cli
test-cpp-cli: build/bin/twsearch
	cargo run --package twsearch-cpp-wrapper \
		--example test-cpp-cli

.PHONY: twsearch-cpp-wrapper-cli
twsearch-cpp-wrapper-cli:
	cargo build --release --package twsearch-cpp-wrapper

.PHONY: test-twsearch-cpp-wrapper-cli
test-twsearch-cpp-wrapper-cli: twsearch-cpp-wrapper-cli
	cargo run --package twsearch-cpp-wrapper \
		--example test-twsearch-cpp-wrapper-cli

# WebAssembly build (single-threaded).  Uses `em++` from the PATH, or, if EMSDK
# names an emsdk checkout, runs that emsdk's em++ with a config and library
# cache under build/, so the emsdk directory need not be activated or written:
#
#    make build-wasm EMSDK=~/emsdk
#
# The output is one ES module with the wasm inlined, for bundling into cubing.js.
#
# `make build-wasm64` builds the same thing with 64-bit memory (Memory64,
# up to 16GB, as Chrome and Firefox allow) into build/wasm64/.  It needs a
# recent emsdk.

WASM_DIR ?= build/wasm
WASM_MEMORY ?= -sMAXIMUM_MEMORY=4GB
ifdef EMSDK
WASM_EM_CONFIG = ${WASM_DIR}/emscripten-config
WASM_CXX = EM_CONFIG=${abspath ${WASM_EM_CONFIG}} EM_CACHE=${abspath ${WASM_DIR}/emcache} \
   ${EMSDK}/upstream/emscripten/em++
else
WASM_CXX = em++
endif
WASM_CXXFLAGS = -O3 -std=c++20 -Wextra -Wall -pedantic -Wsign-compare
WASM_FLAGS = -DTWSEARCH_VERSION=${TWSEARCH_VERSION} -DWASM -DASLIBRARY -I./src/cpp/vendor/cityhash/src
# Asyncify lets a running search yield to the JavaScript event loop so that
# a cancel can be delivered (see src/cpp/cancel.h).  This version of
# Asyncify cannot be combined with wasm exceptions, so there are none;
# error() throws a JavaScript exception instead.
WASM_LDFLAGS = -sMODULARIZE -sEXPORT_ES6 -sSINGLE_FILE \
   -sENVIRONMENT=web,worker -sALLOW_MEMORY_GROWTH ${WASM_MEMORY} \
   -sSTACK_SIZE=5MB -sEXPORT_NAME=createTwsearchModule \
   -sASYNCIFY -sASYNCIFY_IMPORTS=js_pollcancel -sASYNCIFY_IGNORE_INDIRECT \
   -sEXPORTED_FUNCTIONS=_w_args,_w_setksolve,_w_solvescramble \
   -sEXPORTED_RUNTIME_METHODS=ccall
WASM_SOURCE = $(filter-out src/cpp/test.cpp,$(wildcard src/cpp/*.cpp)) \
   src/cpp/wasm/wasmapi.cpp src/cpp/vendor/cityhash/src/city.cc

.PHONY: build-wasm
build-wasm: ${WASM_DIR}/twsearch.mjs

.PHONY: build-wasm64
build-wasm64:
	$(MAKE) build-wasm WASM_DIR=build/wasm64 \
	   WASM_MEMORY="-sMEMORY64 -sMAXIMUM_MEMORY=16GB"

${WASM_DIR}/:
	mkdir -p ${WASM_DIR}/

${WASM_DIR}/emscripten-config: | ${WASM_DIR}/
	printf "LLVM_ROOT = '%s'\nBINARYEN_ROOT = '%s'\nNODE_JS = '%s'\n" \
	   "${EMSDK}/upstream/bin" "${EMSDK}/upstream" "$$(command -v node)" > $@

${WASM_DIR}/twsearch.mjs: $(WASM_SOURCE) $(HSOURCE) src/cpp/wasm/wasmapi.cpp src/js/twsearch-session.mjs src/js/twsearch-worker.mjs Makefiles/cpp.Makefile ${WASM_EM_CONFIG} | ${WASM_DIR}/
	$(WASM_CXX) $(WASM_CXXFLAGS) $(WASM_FLAGS) -o $@ $(WASM_SOURCE) $(WASM_LDFLAGS)
	cp src/js/twsearch-session.mjs src/js/twsearch-worker.mjs ${WASM_DIR}/
