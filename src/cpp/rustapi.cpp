#include "rustapi.h"
#include <string>
#include "wasmapi.h"
#include "rust/cxx.h"

void rust_arg(rust::Str s_) {
  std::string s(s_);
  w_str_arg(s);
}
void rust_setksolve(rust::Str s_) {
  std::string s(s_);
  w_str_setksolve(s);
}
rust::String rust_solvescramble(rust::Str s_) {
  std::string s(s_);
  return w_str_solvescramble(s);
}
rust::String rust_solveposition(rust::Str s_) {
  std::string s(s_);
  return w_str_solveposition(s);
}
void rust_reset() {
  w_reset();
}
