#include "rustapi.h"
#include <string>
#include "wasmapi.h"
#include "twsearch.h"
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
void rust_main_search(rust::Str def_file_, rust::Str scramble_file_) {
  std::string def_file_str(def_file_);
  const char* def_file = def_file_str.c_str();

  const char** scramble_file = NULL;
  if (scramble_file_.length() > 0) {
    std::string scramble_file_str(scramble_file_);
    const char* scramble_file_ptr = scramble_file_str.c_str();
    scramble_file = &scramble_file_ptr;
  }

  main_search(def_file, scramble_file);
}
