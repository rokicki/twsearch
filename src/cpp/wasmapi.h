#ifndef WASMAPI_H
#include <string>

void w_str_arg(std::string s) ;
void w_str_setksolve(std::string s) ;
std::string w_str_solvescramble(std::string s) ;
std::string w_str_solveposition(std::string s) ;

extern "C" {
  void w_arg(const char *s) ;
  void w_setksolve(const char *s) ;
  const char *w_solvescramble(const char *s) ;
  const char *w_solveposition(const char *s) ;
  void w_reset();
}
#define WASMAPI_H
#endif
