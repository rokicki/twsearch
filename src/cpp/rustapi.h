#ifndef RUST_H
#include "rust/cxx.h"
void rust_arg(rust::Str s) ;
void rust_setksolve(rust::Str s) ;
rust::String rust_solvescramble(rust::Str s) ;
rust::String rust_solveposition(rust::Str s) ;
void rust_reset();
// TODO: We can't use `optional` because https://github.com/dtolnay/cxx/issues/87 is unresolved.
// Use the empty string to indicate an empty value for `scramble_file`.
void rust_main_search(rust::Str def_file, rust::Str scramble_file);
#define RUST_H
#endif
