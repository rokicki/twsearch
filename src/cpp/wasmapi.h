#ifndef WASMAPI_H
#include <string>
extern "C" {
   void w_arg(const char *s) ;
   void w_setksolve(const char *s) ;
   const char *w_solvescramble(const char *s) ;
   const char *w_solveposition(const char *s) ;
}
#define WASMAPI_H
#endif
