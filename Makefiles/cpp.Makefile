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
   src/cpp/workchunks.cpp src/cpp/cmds.cpp src/cpp/cmdlineops.cpp subgroup.cpp

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
   build/cpp/totalvar.o build/cpp/vendor/cityhash/city.o

HSOURCE = src/cpp/antipode.h src/cpp/canon.h src/cpp/cmdlineops.h \
   src/cpp/filtermoves.h src/cpp/findalgo.h src/cpp/generatingset.h src/cpp/god.h src/cpp/index.h \
   src/cpp/parsemoves.h src/cpp/prunetable.h src/cpp/puzdef.h src/cpp/readksolve.h src/cpp/solve.h \
   src/cpp/test.h src/cpp/threads.h src/cpp/util.h src/cpp/workchunks.h src/cpp/rotations.h \
   src/cpp/orderedgs.h src/cpp/twsearch.h src/cpp/coset.h src/cpp/descsets.h \
   src/cpp/ordertree.h src/cpp/unrotate.h src/cpp/shorten.h src/cpp/cmds.h \
   src/cpp/totalvar.h src/cpp/subgroup.h

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
