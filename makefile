all: twsearch

CXXFLAGS = -O3 -Wextra -Wall -pedantic -std=c++14 -g -march=native -Wsign-compare
FLAGS = -DUSE_PTHREADS -DHAVE_FFSLL -Isrc -Isrc/cityhash/src
LDFLAGS = -lpthread

CSOURCE = src/antipode.cpp src/calcsymm.cpp src/canon.cpp src/cmdlineops.cpp \
   src/filtermoves.cpp src/findalgo.cpp src/generatingset.cpp src/god.cpp \
   src/index.cpp src/parsemoves.cpp src/prunetable.cpp src/puzdef.cpp \
   src/readksolve.cpp src/solve.cpp src/test.cpp src/threads.cpp \
   src/twsearch.cpp src/util.cpp src/workchunks.cpp src/rotations.cpp \
   src/orderedgs.cpp

HSOURCE = src/antipode.h src/calcsymm.h src/canon.h src/cmdlineops.h \
   src/filtermoves.h src/findalgo.h src/generatingset.h src/god.h src/index.h \
   src/parsemoves.h src/prunetable.h src/puzdef.h src/readksolve.h src/solve.h \
   src/test.h src/threads.h src/util.h src/workchunks.h src/rotations.h \
   src/orderedgs.h

CITYSRC = src/cityhash/src/city.cc

twsearch: $(CSOURCE) $(HSOURCE)
	$(CXX) $(CXXFLAGS) $(FLAGS) -o twsearch $(CSOURCE) $(CITYSRC) $(LDFLAGS)
