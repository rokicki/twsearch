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

.PHONY: describe-version
describe-version:
	@ # TODO: this wastes 0.1 second running `setup-js` a second time when building both C++ and JS targets — can we avoid that?
	@ make setup-js 2>&1 > /dev/null
	@ bun x -- @lgarron-bin/repo version describe

include ./Makefiles/cpp.Makefile
