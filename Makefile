# Filtered to contain only the C++ build.

.PHONY: build
build: twsearch

MAKEFLAGS += -j
CXXFLAGS = -O3 -Wextra -Wall -pedantic -std=c++14 -g -march=native -Wsign-compare
FLAGS = -DUSE_PTHREADS -DHAVE_FFSLL
LDFLAGS = -lpthread

CSOURCE = src/antipode.cpp src/calcsymm.cpp src/canon.cpp src/cmdlineops.cpp \
   src/filtermoves.cpp src/findalgo.cpp src/generatingset.cpp src/god.cpp \
   src/index.cpp src/parsemoves.cpp src/prunetable.cpp src/puzdef.cpp \
   src/readksolve.cpp src/solve.cpp src/test.cpp src/threads.cpp \
   src/twsearch.cpp src/util.cpp src/workchunks.cpp src/rotations.cpp \
   src/orderedgs.cpp src/wasmapi.cpp src/cityhash/src/city.cc src/coset.cpp \
   src/descsets.cpp src/ordertree.cpp src/unrotate.cpp

OBJ = antipode.o calcsymm.o canon.o cmdlineops.o \
   filtermoves.o findalgo.o generatingset.o god.o \
   index.o parsemoves.o prunetable.o puzdef.o \
   readksolve.o solve.o test.o threads.o \
   twsearch.o util.o workchunks.o rotations.o \
   orderedgs.o wasmapi.o city.o coset.o descsets.o \
   ordertree.o unrotate.o

HSOURCE = src/antipode.h src/calcsymm.h src/canon.h src/cmdlineops.h \
   src/filtermoves.h src/findalgo.h src/generatingset.h src/god.h src/index.h \
   src/parsemoves.h src/prunetable.h src/puzdef.h src/readksolve.h src/solve.h \
   src/test.h src/threads.h src/util.h src/workchunks.h src/rotations.h \
   src/orderedgs.h src/wasmapi.h src/twsearch.h src/coset.h src/descsets.h \
   src/ordertree.h src/unrotate.h

%.o: src/%.cpp $(HSOURCE)
	$(CXX) -I./src/cityhash/src -c $(CXXFLAGS) $(FLAGS) $<

%.o: src/cityhash/src/%.cc
	$(CXX) -I./src/cityhash/src -c $(CXXFLAGS) $(FLAGS) $<

.PHONY: clean
clean:
	rm -f *.o twsearch

twsearch: $(OBJ)
	$(CXX) $(CXXFLAGS) -o twsearch $(OBJ) $(LDFLAGS)
