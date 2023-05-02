#ifndef RUST_H
#include "rust/cxx.h"
void rust_arg(rust::Str s) ;
void rust_setksolve(rust::Str s) ;
rust::String rust_solvescramble(rust::Str s) ;
rust::String rust_solveposition(rust::Str s) ;
void rust_reset();
#define RUST_H
#endif
