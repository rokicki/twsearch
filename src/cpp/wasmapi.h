#ifndef WASMAPI_H
#include "rust/cxx.h"
#include <string>
void w_arg(rust::Str s) ;
void w_setksolve(rust::Str s) ;
rust::String w_solvescramble(rust::Str s) ;
rust::String w_solveposition(rust::Str s) ;
#define WASMAPI_H
#endif
