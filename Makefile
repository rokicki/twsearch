# Filtered to contain only the C++ build.

.PHONY: build
build: build/bin/twsearch

.PHONY: clean
clean:
	rm -rf ./build

MAKEFLAGS += -j
CXXFLAGS = -O3 -Wextra -Wall -pedantic -std=c++14 -g -Wsign-compare
FLAGS = -DUSE_PTHREADS -DHAVE_FFSLL
LDFLAGS = -lpthread

CSOURCE = src/cpp/antipode.cpp src/cpp/calcsymm.cpp src/cpp/canon.cpp src/cpp/cmdlineops.cpp \
   src/cpp/filtermoves.cpp src/cpp/findalgo.cpp src/cpp/generatingset.cpp src/cpp/god.cpp \
   src/cpp/index.cpp src/cpp/parsemoves.cpp src/cpp/prunetable.cpp src/cpp/puzdef.cpp \
   src/cpp/readksolve.cpp src/cpp/solve.cpp src/cpp/test.cpp src/cpp/threads.cpp \
   src/cpp/twsearch.cpp src/cpp/util.cpp src/cpp/workchunks.cpp src/cpp/rotations.cpp \
   src/cpp/orderedgs.cpp src/cpp/wasmapi.cpp src/cpp/cityhash/src/city.cc src/cpp/coset.cpp \
   src/cpp/descsets.cpp src/cpp/ordertree.cpp src/cpp/unrotate.cpp src/cpp/shorten.cpp

OBJ = build/cpp/antipode.o build/cpp/calcsymm.o build/cpp/canon.o build/cpp/cmdlineops.o \
   build/cpp/filtermoves.o build/cpp/findalgo.o build/cpp/generatingset.o build/cpp/god.o \
   build/cpp/index.o build/cpp/parsemoves.o build/cpp/prunetable.o build/cpp/puzdef.o \
   build/cpp/readksolve.o build/cpp/solve.o build/cpp/test.o build/cpp/threads.o \
   build/cpp/twsearch.o build/cpp/util.o build/cpp/workchunks.o build/cpp/rotations.o \
   build/cpp/orderedgs.o build/cpp/wasmapi.o build/cpp/city.o build/cpp/coset.o build/cpp/descsets.o \
   build/cpp/ordertree.o build/cpp/unrotate.o build/cpp/shorten.o

HSOURCE = src/cpp/antipode.h src/cpp/calcsymm.h src/cpp/canon.h src/cpp/cmdlineops.h \
   src/cpp/filtermoves.h src/cpp/findalgo.h src/cpp/generatingset.h src/cpp/god.h src/cpp/index.h \
   src/cpp/parsemoves.h src/cpp/prunetable.h src/cpp/puzdef.h src/cpp/readksolve.h src/cpp/solve.h \
   src/cpp/test.h src/cpp/threads.h src/cpp/util.h src/cpp/workchunks.h src/cpp/rotations.h \
   src/cpp/orderedgs.h src/cpp/wasmapi.h src/cpp/twsearch.h src/cpp/coset.h src/cpp/descsets.h \
   src/cpp/ordertree.h src/cpp/unrotate.h src/cpp/shorten.h

build/cpp:
	mkdir -p build/cpp

build/cpp/%.o: src/cpp/%.cpp $(HSOURCE) build/cpp
	$(CXX) -I./src/cpp/cityhash/src -c $(CXXFLAGS) $(FLAGS) $< -o $@

build/cpp/%.o: src/cpp/cityhash/src/%.cc build/cpp
	$(CXX) -I./src/cpp/cityhash/src -c $(CXXFLAGS) $(FLAGS) $< -o $@

build/bin:
	mkdir -p build/bin

build/bin/twsearch: $(OBJ) build/bin
	$(CXX) $(CXXFLAGS) -o build/bin/twsearch $(OBJ) $(LDFLAGS)

# WASM

WASM_CXX = emsdk/upstream/emscripten/em++
WASM_CXXFLAGS = -O3 -fno-exceptions -Wextra -Wall -pedantic -std=c++14 -g -Wsign-compare
WASM_FLAGS = -DHAVE_FFSLL -DWASM -DWASMTEST -DASLIBRARY -s TOTAL_MEMORY=1024MB -Isrc/cpp -Isrc/cpp/cityhash/src
WASM_LDFLAGS = 

build/wasm:
	mkdir -p build/wasm

build/wasm/twsearch.wasm: $(CSOURCE) $(HSOURCE) build/wasm ${WASM_CXX}
	$(WASM_CXX) $(WASM_CXXFLAGS) $(WASM_FLAGS) -o $@ $(CSOURCE) $(WASM_LDFLAGS)

emsdk: ${WASM_CXX}
${WASM_CXX}:
	git clone https://github.com/emscripten-core/emsdk.git
	cd emsdk && ./emsdk install latest
	cd emsdk && ./emsdk activate latest
