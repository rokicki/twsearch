# Filtered to contain only the C++ build.

.PHONY: build
build: build/bin/twsearch

.PHONY: clean
clean:
	rm -rf ./build

.PHONY: lint
lint: lint-cpp

.PHONY: format
format: format-cpp

include ./Makefiles/cpp.Makefile
