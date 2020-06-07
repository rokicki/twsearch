/*
 *   For WASM use, as an initial test, we use global state to store
 *   our puzdef and pruning table, so our API is just strings everywhere.
 */
#include <sstream>
#include "puzdef.h"
#include "prunetable.h"
#include "solve.h"
#include "twsearch.h"
#include "parsemoves.h"
#include "cmdlineops.h"
struct wasmdata {
   puzdef pd ;
   prunetable pt ;
   int havepd, havept ;
} wasmdata ;
extern "C" void w_args(const char *s_) {
   string s(s_) ;
   const char *argva[4] ;
   const char **argv = argva ;
   int argc = 0 ;
   argv[0] = "w_args" ;
   int space = 0 ;
   string scopy ;
   while (space < (int)s.size() && s[space] != ' ')
      space++ ;
   if (space >= (int)s.size()) {
      argv[1] = s.c_str() ;
      argv[2] = 0 ;
      argc = 2 ;
   } else {
      scopy = s ;
      scopy[space] = 0 ;
      argv[1] = scopy.c_str() ;
      argv[2] = scopy.c_str() + space + 1 ;
      argv[3] = 0 ;
      argc = 3 ;
   }
   processargs(argc, argv) ;
   if (argc != 1)
      error("! error processing argument") ;
}
void checkprunetable() {
   if (!wasmdata.havepd)
      error(
     "! you must set the ksolve definition before building a pruning table") ;
   if (!wasmdata.havept) {
      wasmdata.pt = prunetable(wasmdata.pd, maxmem) ;
      wasmdata.havept = 1 ;
   }
}
extern "C" void w_setksolve(const char *s_) {
   string s(s_) ;
   wasmdata.pd = makepuzdef(s) ;
   wasmdata.havepd = 1 ;
}
extern "C" const char *w_solvescramble(const char *s) {
   lastsolution = "--no solution--" ;
   checkprunetable() ;
   puzdef &pd = wasmdata.pd ;
   stacksetval p1(pd) ;
   vector<setval> movelist = parsemovelist_generously(pd, s) ;
   for (auto &m : movelist)
      domove(pd, p1, m) ;
   string noname("NoScrambleName") ;
   solveit(pd, wasmdata.pt, noname, p1) ;
   return lastsolution.c_str() ;
}
extern "C" const char *w_solveposition(const char *s) {
   lastsolution = "--no solution--" ;
   checkprunetable() ;
   stringstream is(s) ;
   processscrambles(&is, wasmdata.pd, wasmdata.pt, 0) ;
   return lastsolution.c_str() ;
}
#ifdef WASMTEST
const char *twsfile = 
"Name PuzzleGeometryPuzzle\n"
"\n"
"Set CORNER 8 3\n"
"\n"
"Solved\n"
"CORNER\n"
"1 2 3 4 5 6 7 8\n"
"0 0 0 0 0 0 0 0\n"
"End\n"
"\n"
"Move F\n"
"CORNER\n"
"7 1 3 2 5 6 4 8\n"
"2 1 0 2 0 0 1 0\n"
"End\n"
"\n"
"Move B\n"
"CORNER\n"
"1 2 5 4 8 3 7 6\n"
"0 0 1 0 2 2 0 1\n"
"End\n"
"\n"
"Move D\n"
"CORNER\n"
"1 4 3 8 2 6 7 5\n"
"0 0 0 0 0 0 0 0\n"
"End\n"
"\n"
"Move U\n"
"CORNER\n"
"3 2 6 4 5 7 1 8\n"
"0 0 0 0 0 0 0 0\n"
"End\n"
"\n"
"Move L\n"
"CORNER\n"
"1 2 3 7 5 8 6 4\n"
"0 0 0 1 0 1 2 2\n"
"End\n"
"\n"
"Move R\n"
"CORNER\n"
"2 5 1 4 3 6 7 8\n"
"1 2 2 0 1 0 0 0\n"
"End\n" ;
const char *scrfile = 
"Scramble CornerTwist\n"
"CORNER\n"
"1 2 3 4 5 6 7 8\n"
"1 2 0 0 0 0 0 0\n"
"End\n" ;
int main() {
   w_args("--nowrite") ; // unnecessary if compiled with -DWASM
   w_setksolve(twsfile) ;
   cout << w_solvescramble("U F R D B L U F R D B L U F R D B L") << endl ;
   cout << w_solveposition(scrfile) << endl ;
   cout << w_solvescramble("U F R D B L U F R D B L U F R D B L") << endl ;
   cout << w_solveposition(scrfile) << endl ;
}
#endif
