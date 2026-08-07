# experiments/minrot: prototyping symmetry-reduction ("guess") algorithms
# completely independently of twsearch.
#
#   make extract   -- (re)build and run the data extractor, which links
#                      against twsearch's already-built object files (it
#                      needs the real puzdef/rotations machinery to get
#                      correct rotation-group data -- see extract.cpp)
#                      and writes <puzzle>_rotdata.h / _samples.bin here.
#   make bench     -- build the standalone benchmark (bench.cpp), which
#                      has NO twsearch dependency at all: just the
#                      generated data files above.
#
# Requires ../../build/cpp/*.o to already exist (`make build` at the repo
# root first).

TWSEARCH_ROOT = ../..
TWSEARCH_SRC = $(TWSEARCH_ROOT)/src/cpp
TWSEARCH_OBJDIR = $(TWSEARCH_ROOT)/build/cpp
CXXFLAGS = -O3 -std=c++20 -Wall -Wextra -I$(TWSEARCH_SRC) -I$(TWSEARCH_SRC)/vendor/cityhash/src

# Just the core needed to load a .tws file and compute its rotation group
# -- deliberately not the solve/prune/cmd machinery (those reference
# twsearch.cpp's own globals like cmdhead/gs, which we don't want to drag
# in here).
TWSEARCH_OBJS = $(addprefix $(TWSEARCH_OBJDIR)/, \
   puzdef.o readksolve.o filtermoves.o rotations.o util.o threads.o \
   index.o parsemoves.o) \
   $(TWSEARCH_OBJDIR)/vendor/cityhash/city.o

.PHONY: extract bench clean

extract: extractor
	./extractor

extractor: extract.cpp $(TWSEARCH_OBJS)
	$(CXX) $(CXXFLAGS) -o extractor extract.cpp $(TWSEARCH_OBJS) -lpthread

bench: bench.cpp
	$(CXX) -O3 -std=c++20 -Wall -Wextra -o bench bench.cpp
	./bench

clean:
	rm -f extractor bench *_rotdata.h *_samples.bin
