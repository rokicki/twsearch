# Filtered to contain only the C++ build.

.PHONY: build
build: build/bin/twsearch

.PHONY: all
all: build/bin/twsearch build/esm build-rust

.PHONY: clean
clean:
	rm -rf ./.temp ./build ./src/js/generated-wasm/twsearch.* ./*.dwo ./target

.PHONY: reset
reset: clean
	rm -rf ./emsdk ./node_modules

.PHONY: lint
lint: lint-js

.PHONY: format
format: format-js

TWSEARCH_VERSION=v0.0.0

# MAKEFLAGS += -j
CXXFLAGS = -O3 -Wextra -Wall -pedantic -std=c++14 -g -Wsign-compare
FLAGS = -DTWSEARCH_VERSION=${TWSEARCH_VERSION} -DUSE_PTHREADS -DHAVE_FFSLL
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

build/cpp/:
	mkdir -p build/cpp/

build/cpp/%.o: src/cpp/%.cpp $(HSOURCE) | build/cpp/
	$(CXX) -I./src/cpp/cityhash/src -c $(CXXFLAGS) $(FLAGS) $< -o $@

build/cpp/%.o: src/cpp/cityhash/src/%.cc | build/cpp/
	$(CXX) -I./src/cpp/cityhash/src -c $(CXXFLAGS) $(FLAGS) $< -o $@

build/bin/:
	mkdir -p build/bin/

build/bin/twsearch: $(OBJ) | build/bin/
	$(CXX) $(CXXFLAGS) -o build/bin/twsearch $(OBJ) $(LDFLAGS)

# WASM

WASM_CXX = emsdk/upstream/emscripten/em++
WASM_CXXFLAGS = -O3 -fno-exceptions -Wextra -Wall -pedantic -std=c++14 -Wsign-compare
WASM_FLAGS = -DTWSEARCH_VERSION=${TWSEARCH_VERSION} -DWASM -DASLIBRARY -Isrc/cpp -Isrc/cpp/cityhash/src -sEXPORTED_FUNCTIONS=_w_arg,_w_setksolve,_w_solvescramble,_w_solveposition -sEXPORTED_RUNTIME_METHODS=cwrap
WASM_TEST_FLAGS = -DWASMTEST -sASSERTIONS
WASM_SINGLE_FILE_FLAGS = -sEXPORT_ES6 -sSINGLE_FILE -sALLOW_MEMORY_GROWTH
WASM_LDFLAGS = 

emsdk: ${WASM_CXX}
${WASM_CXX}:
	make emsdk-tip-of-tree

.PHONY: emsdk-latest
emsdk-latest:
	rm -rf ./emsdk
	git clone https://github.com/emscripten-core/emsdk.git
	cd emsdk && ./emsdk install latest
	cd emsdk && ./emsdk activate latest

.PHONY: emsdk-tip-of-tree
emsdk-tip-of-tree:
	rm -rf ./emsdk
	git clone https://github.com/emscripten-core/emsdk.git
	cd emsdk && ./emsdk install tot
	cd emsdk && ./emsdk activate tot

build/wasm-test/:
	mkdir -p build/wasm-test/

build/wasm-test/twsearch-test.wasm: $(CSOURCE) $(HSOURCE) build/wasm-test/ ${WASM_CXX}
	$(WASM_CXX) $(WASM_CXXFLAGS) $(WASM_FLAGS) $(WASM_TEST_FLAGS) -o $@ $(CSOURCE) $(WASM_LDFLAGS) -DWASMTEST

build/wasm-single-file/:
	mkdir -p build/wasm-single-file/

build/wasm-single-file/twsearch.mjs: $(CSOURCE) $(HSOURCE) build/wasm-single-file/ ${WASM_CXX}
	$(WASM_CXX) $(WASM_CXXFLAGS) $(WASM_FLAGS) $(WASM_SINGLE_FILE_FLAGS) -o $@ $(CSOURCE) $(WASM_LDFLAGS)

# JS

node_modules:
	npm install

ESBUILD_COMMON_ARGS = \
		--format=esm --target=es2020 \
		--bundle --splitting \
		--external:path --external:fs --external:module \
		--external:node:* \

.PHONY: dev
dev: build/wasm-single-file/twsearch.mjs node_modules
	npx esbuild ${ESBUILD_COMMON_ARGS} \
		--sourcemap \
		--servedir=src/js/dev \
		src/js/dev/*.ts

.PHONY: build/esm
build/esm: build/wasm-single-file/twsearch.mjs node_modules
	npx esbuild ${ESBUILD_COMMON_ARGS} \
		--external:cubing \
		--outdir=build/esm src/js/index.ts
	mkdir -p ./.temp
	mv build/esm/index.js ./.temp/index.js
	echo "console.info(\"Loading twsearch ${TWSEARCH_VERSION}\");" > build/esm/index.js
	cat "./.temp/index.js" >> build/esm/index.js

.PHONY: build/esm-test
build/esm-test: build/wasm-single-file/twsearch.mjs node_modules
	npx esbuild ${ESBUILD_COMMON_ARGS} \
		--external:cubing \
		--outdir=build/esm-test \
		src/js/dev/test.ts

.PHONY: test-wasm
test-wasm: build/wasm-test/twsearch-test.wasm
	wasmer build/wasm-test/twsearch-test.wasm

.PHONY: test-build-js
test-build-js: build/esm-test
	node build/esm-test/test.js

.PHONY: lint-js
lint-js:
	npx rome check src/js/**/*.ts

.PHONY: format-js
format-js:
	npx rome format src/js/**/*.ts

# Rust

.PHONY: dev-rust
dev-rust:
	cargo run -- \
		serve \
		--check-before-solve \
		--random-start

.PHONY: dev-rust-40G
dev-rust-40G:
	cargo run -- \
		serve \
		--check-before-solve \
		--random-start \
		--memory-mb 40960

.PHONY: build-rust
build-rust:
	cargo build --release

.PHONY: lint-rust
lint-rust:
	cargo clippy
