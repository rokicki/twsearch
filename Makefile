# Filtered to contain only the C++ build.

.PHONY: build
build: build/bin/twsearch

.PHONY: all
all: build/bin/twsearch build/esm build-rust

.PHONY: setup
setup: node_modules
	cargo install cargo-run-bin
	@wasmer -V > /dev/null || echo "\nEnsure \`wasmer\` is installed: https://docs.wasmer.io/install\n"

.PHONY: test
test: \
	test-warning \
	lint \
	test-cpp-cli \
	test-twsearch-cpp-wrapper-cli \
	test-rust \
	test-rust-ffi \
	benchmark-rust

.PHONY: test-warning
test-warning:
	@echo "Warning: tests are slow to run right now."

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

# Rust testing

.PHONY: test-rust
test-rust: test-rust-lib test-rust-wasm test-rust-ffi

.PHONY: test-rust-lib
test-rust-lib:
	cargo test
	cargo run --release --example random_scramble_for_event

.PHONY: benchmark-rust
benchmark-rust:
	cargo run --release -- benchmark samples/json/benchmark/benchmark-3x3x3.def.json

.PHONY: clean
clean:
	rm -rf ./.temp ./build ./dist ./src/js/generated-wasm/twsearch.* ./*.dwo ./target ./package-lock.json

.PHONY: cpp-clean
cpp-clean:
	rm -rf ./build

.PHONY: reset
reset: clean
	rm -rf ./emsdk ./node_modules ./target

.PHONY: lint
lint: lint-cpp lint-js lint-rust

.PHONY: lint-cpp
lint-cpp:
	find ./src/cpp -iname "*.h" -o -iname "*.cpp" | xargs clang-format --dry-run -Werror

.PHONY: format
format: format-cpp format-js

.PHONY: format-cpp
format-cpp:
	find ./src/cpp -iname "*.h" -o -iname "*.cpp" | xargs clang-format -i

.PHONY: publish
publish: test-rust publish-rust

TWSEARCH_VERSION=v0.0.0

# MAKEFLAGS += -j
CXXFLAGS = -O3 -Warray-bounds -Wextra -Wall -pedantic -std=c++17 -g -Wsign-compare
FLAGS = -DTWSEARCH_VERSION=${TWSEARCH_VERSION} -DUSE_PTHREADS
LDFLAGS = -lpthread

BASESOURCE = src/cpp/canon.cpp src/cpp/cityhash/src/city.cc \
   src/cpp/filtermoves.cpp src/cpp/generatingset.cpp src/cpp/index.cpp \
   src/cpp/parsemoves.cpp src/cpp/prunetable.cpp src/cpp/puzdef.cpp \
   src/cpp/readksolve.cpp src/cpp/rotations.cpp src/cpp/solve.cpp \
   src/cpp/threads.cpp src/cpp/twsearch.cpp src/cpp/util.cpp \
   src/cpp/workchunks.cpp src/cpp/cmds.cpp src/cpp/cmdlineops.cpp

FFISOURCE = src/cpp/ffi/ffi_api.cpp src/cpp/ffi/wasm_api.cpp

EXTRASOURCE = src/cpp/antipode.cpp src/cpp/calcsymm.cpp \
   src/cpp/coset.cpp src/cpp/descsets.cpp \
   src/cpp/findalgo.cpp src/cpp/god.cpp src/cpp/orderedgs.cpp \
   src/cpp/ordertree.cpp src/cpp/shorten.cpp src/cpp/unrotate.cpp \
   src/cpp/test.cpp src/cpp/totalvar.cpp

CSOURCE = $(BASESOURCE) $(FFISOURCE) $(EXTRASOURCE)

WASMSOURCE = $(BASESOURCE) $(FFISOURCE)

OBJ = build/cpp/antipode.o build/cpp/calcsymm.o build/cpp/canon.o build/cpp/cmdlineops.o \
   build/cpp/filtermoves.o build/cpp/findalgo.o build/cpp/generatingset.o build/cpp/god.o \
   build/cpp/index.o build/cpp/parsemoves.o build/cpp/prunetable.o build/cpp/puzdef.o \
   build/cpp/readksolve.o build/cpp/solve.o build/cpp/test.o build/cpp/threads.o \
   build/cpp/twsearch.o build/cpp/util.o build/cpp/workchunks.o build/cpp/rotations.o \
   build/cpp/orderedgs.o build/cpp/ffi/ffi_api.o build/cpp/ffi/wasm_api.o build/cpp/city.o build/cpp/coset.o build/cpp/descsets.o \
   build/cpp/ordertree.o build/cpp/unrotate.o build/cpp/shorten.o build/cpp/cmds.o \
   build/cpp/totalvar.o

HSOURCE = src/cpp/antipode.h src/cpp/calcsymm.h src/cpp/canon.h src/cpp/cmdlineops.h \
   src/cpp/filtermoves.h src/cpp/findalgo.h src/cpp/generatingset.h src/cpp/god.h src/cpp/index.h \
   src/cpp/parsemoves.h src/cpp/prunetable.h src/cpp/puzdef.h src/cpp/readksolve.h src/cpp/solve.h \
   src/cpp/test.h src/cpp/threads.h src/cpp/util.h src/cpp/workchunks.h src/cpp/rotations.h \
   src/cpp/orderedgs.h src/cpp/ffi/ffi_api.h src/cpp/ffi/wasm_api.h src/cpp/twsearch.h src/cpp/coset.h src/cpp/descsets.h \
   src/cpp/ordertree.h src/cpp/unrotate.h src/cpp/shorten.h src/cpp/cmds.h \
   src/cpp/totalvar.h

build/cpp/ffi/:
	mkdir -p build/cpp/ffi/

build/cpp/%.o: src/cpp/%.cpp $(HSOURCE) | build/cpp/ffi/
	$(CXX) -I./src/cpp/cityhash/src -c $(CXXFLAGS) $(FLAGS) $< -o $@

build/cpp/%.o: src/cpp/cityhash/src/%.cc | build/cpp/ffi/
	$(CXX) -I./src/cpp/cityhash/src -c $(CXXFLAGS) $(FLAGS) $< -o $@

build/bin/:
	mkdir -p build/bin/

build/bin/twsearch: $(OBJ) | build/bin/
	$(CXX) $(CXXFLAGS) -o build/bin/twsearch $(OBJ) $(LDFLAGS)

# WASM

WASM_CXX = emsdk/upstream/emscripten/em++
WASM_CXXFLAGS = -O3 -fno-exceptions -Wextra -Wall -pedantic -std=c++17 -Wsign-compare
WASM_COMMON_FLAGS = -DTWSEARCH_VERSION=${TWSEARCH_VERSION} -DWASM -DASLIBRARY -Isrc/cpp -Isrc/cpp/cityhash/src -sEXPORTED_FUNCTIONS=_wasm_api_set_arg,_wasm_api_set_kpuzzle_definition,_wasm_api_solve_scramble,_wasm_api_solve_position -sEXPORTED_RUNTIME_METHODS=cwrap -sALLOW_MEMORY_GROWTH
WASM_TEST_FLAGS = -DWASMTEST -sASSERTIONS
WASM_ESM_BASE64_SINGLE_FILE_FLAGS = -sEXPORT_ES6 -sSINGLE_FILE
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

build/wasm-test/twsearch-test.wasm: $(WASMSOURCE) $(HSOURCE) build/wasm-test/ ${WASM_CXX}
	$(WASM_CXX) $(WASM_CXXFLAGS) $(WASM_COMMON_FLAGS) $(WASM_TEST_FLAGS) -o $@ $(WASMSOURCE) $(WASM_LDFLAGS) -DWASMTEST

build/wasm-single-file/:
	mkdir -p build/wasm-single-file/

build/wasm-single-file/twsearch.mjs: $(WASMSOURCE) $(HSOURCE) build/wasm-single-file/ ${WASM_CXX}
	$(WASM_CXX) $(WASM_CXXFLAGS) $(WASM_COMMON_FLAGS) $(WASM_ESM_BASE64_SINGLE_FILE_FLAGS) -o $@ $(WASMSOURCE) $(WASM_LDFLAGS)

# JS

node_modules:
	bun install

ESBUILD_COMMON_ARGS = \
		--format=esm --target=es2020 \
		--bundle --splitting \
		--external:path --external:fs --external:module \
		--external:node:* \

.PHONY: dev
dev: build/wasm-single-file/twsearch.mjs node_modules
	bun x esbuild ${ESBUILD_COMMON_ARGS} \
		--sourcemap \
		--servedir=src/js/dev \
		src/js/dev/*.ts

.PHONY: build/esm
build/esm: build/wasm-single-file/twsearch.mjs node_modules
	bun x esbuild ${ESBUILD_COMMON_ARGS} \
		--external:cubing \
		--outdir=build/esm src/js/index.ts
	mkdir -p ./.temp
	mv build/esm/index.js ./.temp/index.js
	echo "console.info(\"Loading twsearch ${TWSEARCH_VERSION}\");" > build/esm/index.js
	cat "./.temp/index.js" >> build/esm/index.js

.PHONY: build/esm-test
build/esm-test: build/wasm-single-file/twsearch.mjs node_modules
	bun x esbuild ${ESBUILD_COMMON_ARGS} \
		--external:cubing \
		--outdir=build/esm-test \
		src/js/dev/test.ts

.PHONY: test-wasm
test-wasm: build/wasm-test/twsearch-test.wasm
	wasmer build/wasm-test/twsearch-test.wasm || echo "This test is known to fail due to an unknown import of `_tzset_js`."

.PHONY: test-build-js
test-build-js: build/esm-test
	bun build/esm-test/test.js

.PHONY: lint-js
lint-js:
	bun x @biomejs/biome check ./script ./src/js/**/*.ts ./**/*.json

.PHONY: format-js
format-js:
	bun x @biomejs/biome format ./script ./src/js/**/*.ts ./**/*.json

# Rust

.PHONY: dev-rust
dev-rust:
	cargo run --release -- serve

.PHONY: dev-rust-40G
dev-rust-40G:
	cargo run --release -- serve --memory-MiB 40960

.PHONY: build-rust
build-rust:
	cargo build --release

.PHONY: lint-rust
lint-rust:
	cargo clippy

.PHONY: publish-rust
publish-rust: publish-rust-main publish-rust-ffi

.PHONY: publish-rust-main
publish-rust-main:
	@echo "WARNING: will fall back to `--no-verify` due to https://github.com/rust-lang/cargo/issues/8407" # TODO
	cargo publish --package twsearch || cargo publish --package twsearch --no-verify

# Rust WASM

.PHONY: build-rust-wasm
build-rust-wasm:
	rm -rf "./.temp/rust-wasm"
	cargo bin wasm-pack build --release --target web --out-dir "../../.temp/rust-wasm" src/rs-wasm
	bun script/node-esm-compat.ts
	bun run "./script/build-wasm-package.ts"

.PHONY: test-rust-wasm
test-rust-wasm: build-rust-wasm
	bun "script/test-dist-wasm.js"

# Rust FFI

.PHONY: build-rust-ffi
build-rust-ffi:
	cargo build --release --package twsearch-ffi
	mkdir -p "./.temp"
	cargo bin cbindgen --crate twsearch-ffi --lang c --cpp-compat --output "./.temp/twsearch-ffi.h" # TODO: install `cbindgen`
	cat "./.temp/twsearch-ffi.h" | sed "s#\[..\];#;#g" | sed "s#\[.\];#;#g" | sed "s#const uint8_t#const char#g" > "./target/release/libtwsearch_ffi.h"

.PHONY: test-rust-ffi # TODO: non-PHONY?
test-rust-ffi: test-rust-ffi-rs test-rust-ffi-js test-rust-ffi-c

.PHONY: test-rust-ffi-rs
test-rust-ffi-rs: build-rust-ffi
	cargo test --package twsearch-ffi

.PHONY: test-rust-ffi-js
test-rust-ffi-js: build-rust-ffi
	bun run "src/rs-ffi/test/js_test.ts"

.PHONY: test-rust-ffi-c
test-rust-ffi-c: build-rust-ffi
	gcc -o src/rs-ffi/test/c_test.bin -L./target/release src/rs-ffi/test/c_test.c -ltwsearch_ffi
	env LD_LIBRARY_PATH=./target/release src/rs-ffi/test/c_test.bin

.PHONY: publish-rust-ffi
publish-rust-ffi:
	@echo "WARNING: will fall back to `--no-verify` due to https://github.com/rust-lang/cargo/issues/8407" # TODO
	cargo publish --package twsearch-ffi || cargo publish --package twsearch-ffi --no-verify
