/*
 *   Stand-ins for source files the WebAssembly build leaves out (WASM_OMIT
 *   in Makefiles/cpp.Makefile), for the few of their functions that the
 *   remaining code still refers to.
 */
#include "../cmdlineops.h"
#include "../prunetable.h"
#include "../puzdef.h"
#include "../subgroup.h"
#include "../util.h"

// pruneio.cpp: pruning tables are never read from or written to files.
int prunetable::readpt(const puzdef &) { return 0; }
void prunetable::writept(const puzdef &) {}

// subgroup.cpp: the --subgroup option isn't available, so this stays null.
const char *subgroupmovelist;
void runsubgroup(puzdef &) {
  error("! --subgroup is not available in this build");
}

// cmdlineops.cpp: --scramblealg isn't available (scrambles come as text).
void solvecmdline(puzdef &, const char *, generatingset *) {
  error("! --scramblealg is not available in this build");
}
