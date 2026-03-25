TWSEARCH_VERSION=v0.0.0

# Build variant: release (default) or asan
BUILD ?= release

# MAKEFLAGS += -j
ifeq ($(BUILD),asan)
  CXXFLAGS = -fsanitize=address,undefined -O1 -fno-omit-frame-pointer \
      -Warray-bounds -Wextra -Wall -pedantic -std=c++20 -g -Wsign-compare
  LDFLAGS = -lpthread -fsanitize=address,undefined
  BUILDDIR = build-asan
else
  CXXFLAGS = -O3 -Warray-bounds -Wextra -Wall -pedantic \
      -std=c++20 -g -Wsign-compare
  LDFLAGS = -lpthread
  BUILDDIR = build
endif
FLAGS = -DTWSEARCH_VERSION=${TWSEARCH_VERSION} -DUSE_PTHREADS -DUSE_PPQSORT

.PHONY: build-cpp
build-cpp: $(BUILDDIR)/bin/twsearch

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

OBJ = \
   $(BUILDDIR)/cpp/antipode.o \
   $(BUILDDIR)/cpp/canon.o \
   $(BUILDDIR)/cpp/cmdlineops.o \
   $(BUILDDIR)/cpp/filtermoves.o \
   $(BUILDDIR)/cpp/findalgo.o \
   $(BUILDDIR)/cpp/generatingset.o \
   $(BUILDDIR)/cpp/god.o \
   $(BUILDDIR)/cpp/index.o \
   $(BUILDDIR)/cpp/parsemoves.o \
   $(BUILDDIR)/cpp/prunetable.o \
   $(BUILDDIR)/cpp/pruneio.o \
   $(BUILDDIR)/cpp/puzdef.o \
   $(BUILDDIR)/cpp/readksolve.o \
   $(BUILDDIR)/cpp/solve.o \
   $(BUILDDIR)/cpp/test.o \
   $(BUILDDIR)/cpp/threads.o \
   $(BUILDDIR)/cpp/twsearch.o \
   $(BUILDDIR)/cpp/util.o \
   $(BUILDDIR)/cpp/workchunks.o \
   $(BUILDDIR)/cpp/rotations.o \
   $(BUILDDIR)/cpp/orderedgs.o \
   $(BUILDDIR)/cpp/coset.o \
   $(BUILDDIR)/cpp/descsets.o \
   $(BUILDDIR)/cpp/ordertree.o \
   $(BUILDDIR)/cpp/unrotate.o \
   $(BUILDDIR)/cpp/shorten.o \
   $(BUILDDIR)/cpp/cmds.o \
   $(BUILDDIR)/cpp/beamsearch.o \
   $(BUILDDIR)/cpp/subgroup.o \
   $(BUILDDIR)/cpp/totalvar.o \
   $(BUILDDIR)/cpp/multiphase.o \
   $(BUILDDIR)/cpp/vendor/cityhash/city.o

HSOURCE = src/cpp/antipode.h src/cpp/canon.h src/cpp/cmdlineops.h \
   src/cpp/filtermoves.h src/cpp/findalgo.h src/cpp/generatingset.h src/cpp/god.h src/cpp/index.h \
   src/cpp/parsemoves.h src/cpp/prunetable.h src/cpp/puzdef.h src/cpp/readksolve.h src/cpp/solve.h \
   src/cpp/test.h src/cpp/threads.h src/cpp/util.h src/cpp/workchunks.h src/cpp/rotations.h \
   src/cpp/orderedgs.h src/cpp/twsearch.h src/cpp/coset.h src/cpp/descsets.h \
   src/cpp/ordertree.h src/cpp/unrotate.h src/cpp/shorten.h src/cpp/cmds.h \
   src/cpp/totalvar.h src/cpp/subgroup.h src/cpp/multiphase.h

$(BUILDDIR)/cpp:
	mkdir -p $(BUILDDIR)/cpp

$(BUILDDIR)/cpp/%.o: src/cpp/%.cpp Makefiles/cpp.Makefile $(HSOURCE) \
    | $(BUILDDIR)/cpp
	$(CXX) -I./src/cpp/vendor/cityhash/src \
	    -c $(CXXFLAGS) $(FLAGS) $< -o $@

$(BUILDDIR)/cpp/vendor/cityhash:
	mkdir -p $(BUILDDIR)/cpp/vendor/cityhash

$(BUILDDIR)/cpp/vendor/cityhash/%.o: \
    src/cpp/vendor/cityhash/src/%.cc \
    Makefiles/cpp.Makefile \
    | $(BUILDDIR)/cpp/vendor/cityhash
	$(CXX) -I./src/cpp/vendor/cityhash/src \
	    -c $(CXXFLAGS) $(FLAGS) $< -o $@

$(BUILDDIR)/bin/:
	mkdir -p $(BUILDDIR)/bin/

$(BUILDDIR)/bin/twsearch: $(OBJ) Makefiles/cpp.Makefile \
    | $(BUILDDIR)/bin/
	$(CXX) $(CXXFLAGS) -o $(BUILDDIR)/bin/twsearch $(OBJ) $(LDFLAGS)

.PHONY: lint-cpp
lint-cpp:
	find ./src/cpp -iname "*.h" -o -iname "*.cpp" | grep -v ppqsort | xargs clang-format --dry-run -Werror

.PHONY: format-cpp
format-cpp:
	find ./src/cpp -iname "*.h" -o -iname "*.cpp" | grep -v ppqsort | xargs clang-format -i

.PHONY: cpp-clean
cpp-clean:
	rm -rf ./$(BUILDDIR)

# C++ and `twsearch-cpp-wrapper` testing

.PHONY: test-cpp-cli
test-cpp-cli: $(BUILDDIR)/bin/twsearch
	cargo run --package twsearch-cpp-wrapper \
		--example test-cpp-cli

.PHONY: twsearch-cpp-wrapper-cli
twsearch-cpp-wrapper-cli:
	cargo build --release --package twsearch-cpp-wrapper

.PHONY: test-twsearch-cpp-wrapper-cli
test-twsearch-cpp-wrapper-cli: twsearch-cpp-wrapper-cli
	cargo run --package twsearch-cpp-wrapper \
		--example test-twsearch-cpp-wrapper-cli
