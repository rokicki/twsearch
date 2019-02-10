#include <iostream>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <math.h>
#include <cstdio>
#include <functional>
#include "city.h"
#include "util.h"
#include "threads.h"
#include "puzdef.h"
#include "generatingset.h"
#include "readksolve.h"
#include "index.h"
#include "antipode.h"
#include "canon.h"
#include "god.h"
#include "findalgo.h"
#include "workchunks.h"
#include "prunetable.h"
#include "solve.h"
#include "test.h"
#include "parsemoves.h"
#include "filtermoves.h"
#include "parsemoves.h"
#include "cmdlineops.h"
using namespace std ;
int checkbeforesolve ;
generatingset *gs ;
int bestsolve = 1000000 ;
int optmaxdepth = 0 ;
void dophase2(const puzdef &pd, setval scr, setval p1sol, prunetable &pt,
              const char *p1str) {
   stacksetval p2(pd) ;
   if (optmaxdepth == 0)
      optmaxdepth = maxdepth ;
   pd.mul(scr, p1sol, p2) ;
   maxdepth = min(optmaxdepth - globalinputmovecount,
                  bestsolve - globalinputmovecount - 1) ;
   int r = solve(pd, pt, p2, gs) ;
   if (r >= 0) {
      cout << "Phase one was " << p1str << endl ;
      bestsolve = r + globalinputmovecount ;
      cout << "Found a solution totaling " << bestsolve << " moves." << endl ;
   }
}
int dogod, docanon, doalgo, dosolvetest, dotimingtest, douniq,
    dosolvelines, doorder, doshowmoves, doshowpositions, genrand,
    checksolvable, doss ;
const char *scramblealgo = 0 ;
const char *legalmovelist = 0 ;
int main(int argc, const char **argv) {
   int seed = 0 ;
   int forcearray = 0 ;
   init_util() ;
   cout << "# This is twsearch 0.1 (C) 2018 Tomas Rokicki." << endl ;
   cout << "#" ;
   for (int i=0; i<argc; i++)
      cout << " " << argv[i] ;
   cout << endl << flush ;
   while (argc > 1 && argv[1][0] == '-') {
      argc-- ;
      argv++ ;
      if (argv[0][1] == '-') {
         if (strcmp(argv[0], "--moves") == 0) {
            legalmovelist = argv[1] ;
            argc-- ;
            argv++ ;
         } else if (strcmp(argv[0], "--showmoves") == 0) {
            doshowmoves++ ;
         } else if (strcmp(argv[0], "--showpositions") == 0) {
            doshowpositions++ ;
         } else if (strcmp(argv[0], "--newcanon") == 0) {
            ccount = atol(argv[1]) ;
            argc-- ;
            argv++ ;
         } else if (strcmp(argv[0], "--nocorners") == 0) {
            nocorners++ ;
         } else if (strcmp(argv[0], "--nocenters") == 0) {
            nocenters++ ;
         } else if (strcmp(argv[0], "--noorientation") == 0) {
            ignoreori = 1 ;
         } else if (strcmp(argv[0], "--noearlysolutions") == 0) {
            noearlysolutions = 1 ;
         } else if (strcmp(argv[0], "--checkbeforesolve") == 0) {
            checkbeforesolve = 1 ;
         } else if (strcmp(argv[0], "--orientationgroup") == 0) {
            origroup = atol(argv[1]) ;
            argc-- ;
            argv++ ;
         } else if (strcmp(argv[0], "--noedges") == 0) {
            noedges++ ;
         } else if (strcmp(argv[0], "--scramblealg") == 0) {
            scramblealgo = argv[1] ;
            argc-- ;
            argv++ ;
         } else if (strcmp(argv[0], "--schreiersims") == 0) {
            doss = 1 ;
         } else {
            error("! Argument not understood ", argv[0]) ;
         }
      } else {
         switch (argv[0][1]) {
case 'q':
            quarter++ ;
            break ;
case 'v':
            verbose++ ;
            if (argv[0][2] != 0)
               verbose = argv[0][2] - '0' ;
            break ;
case 'm':
case 'd':
            maxdepth = atol(argv[1]) ;
            argc-- ;
            argv++ ;
            break ;
case 'r':
            genrand = 1 ;
            break ;
case 'R':
            seed = atol(argv[1]) ;
            argc-- ;
            argv++ ;
            break ;
case 'M':
            maxmem = 1048576 * atoll(argv[1]) ;
            argc-- ;
            argv++ ;
            break ;
case 'y':
            symcoordgoal = atoll(argv[1]) ;
            if (symcoordgoal <= 0)
               symcoordgoal = 1 ;
            argc-- ;
            argv++ ;
            break ;
case 'c':
            solutionsneeded = atoll(argv[1]) ;
            argc-- ;
            argv++ ;
            break ;
case 'g':
            dogod++ ;
            break ;
case 'o':
            doorder++ ;
            break ;
case 'u':
            douniq++ ;
            break ;
case 's':
            dosolvelines++ ;
            break ;
case 'C':
            docanon++ ;
            break ;
case 'F':
            forcearray++ ;
            break ;
case 'a':
            antipodecount = atoll(argv[1]) ;
            argc-- ;
            argv++ ;
            break ;
case 'A':
            if (argv[0][2] == 0 || argv[0][2] == '1')
               doalgo = 1 ;
            else if (argv[0][2] == '2')
               doalgo = 2 ;
            else if (argv[0][2] == '3')
               doalgo = 3 ;
            else
               error("! bad -A value") ;
            break ;
case 'T':
            dotimingtest++ ;
            break ;
case 'S':
            dosolvetest++ ;
            break ;
case 't':
            numthreads = atol(argv[1]) ;
            if (numthreads > MAXTHREADS)
               error("Numthreads cannot be more than ", to_string(MAXTHREADS)) ;
            argc-- ;
            argv++ ;
            break ;
case '2':
            phase2 = 1 ;
            break ;
default:
            error("! did not argument ", argv[0]) ;
         }
      }
   }
   if (seed)
      srand48(seed) ;
   else
      srand48(time(0)) ;
   if (argc <= 1)
      error("! please provide a twsearch file name on the command line") ;
   FILE *f = fopen(argv[1], "r") ;
   if (f == 0)
      error("! could not open file ", argv[1]) ;
   int sawdot = 0 ;
   for (int i=0; argv[1][i]; i++) {
      if (argv[1][i] == '.')
         sawdot = 1 ;
      else if (argv[1][i] == '/' || argv[1][i] == '\\') {
         sawdot = 0 ;
         inputbasename.clear() ;
      } else if (!sawdot)
         inputbasename.push_back(argv[1][i]) ;
   }
   puzdef pd = readdef(f) ;
   addmovepowers(pd) ;
   if (legalmovelist)
      filtermovelist(pd, legalmovelist) ;
   if (nocorners)
      pd.addoptionssum("nocorners") ;
   if (nocenters)
      pd.addoptionssum("nocenters") ;
   if (noedges)
      pd.addoptionssum("noedges") ;
   if (doss || checkbeforesolve)
      gs = new generatingset(pd) ;
   if (genrand) {
      showrandompos(pd) ;
      return 0 ;
   }
   calculatesizes(pd) ;
   calclooseper(pd) ;
   if (ccount == 0)
      makecanonstates(pd) ;
   else
      makecanonstates2(pd) ;
   cout << "Calculated canonical states in " << duration() << endl << flush ;
   showcanon(pd, docanon) ;
//   gensymm(pd) ;
   if (dogod) {
      int statesfit2 = pd.logstates <= 50 && ((ll)(pd.llstates >> 2)) <= maxmem ;
      int statesfitsa = forcearray ||
          (pd.logstates <= 50 &&
             ((ll)(pd.llstates * sizeof(loosetype) * looseper) <= maxmem)) ;
      if (statesfit2 && pd.canpackdense()) {
         cout << "Using twobit arrays." << endl ;
         dotwobitgod2(pd) ;
      } else if (statesfitsa) {
         cout << "Using sorting bfs and arrays." << endl ;
         doarraygod(pd) ;
      } else {
         cout << "Using canonical sequences and arrays." << endl ;
         doarraygod2(pd) ;
      }
   }
   if (doalgo == 1)
      findalgos(pd) ;
   if (doalgo == 2)
      findalgos2(pd) ;
   if (doalgo == 3)
      findalgos3(pd) ;
   if (dosolvetest)
      solvetest(pd, gs) ;
   if (dotimingtest)
      timingtest(pd) ;
   if (! phase2 && scramblealgo)
      solvecmdline(pd, scramblealgo) ;
   if (douniq)
      processlines(pd, uniqit) ;
   if (doorder)
      processlines2(pd, orderit) ;
   if (doshowmoves)
      processlines2(pd, emitmove) ;
   if (doshowpositions)
      processlines(pd, emitposition) ;
   if (dosolvelines) {
      prunetable pt(pd, maxmem) ;
      string emptys ;
      processlines(pd, [&](const puzdef &pd, setval p, const char *) {
                          solveit(pd, pt, emptys, p) ;
                       }) ;
   }
   if (phase2) {
      if (argc <= 2 && !scramblealgo)
         error("! need a scramble file for phase 2") ;
      stacksetval scr(pd) ;
      if (scramblealgo) {
         pd.assignpos(scr, pd.solved) ;
         vector<setval> movelist = parsemovelist_generously(pd, scramblealgo) ;
         for (int i=0; i<(int)movelist.size(); i++)
            domove(pd, scr, movelist[i]) ;
      } else {
         f = fopen(argv[2], "r") ;
         if (f == 0)
            error("! could not open scramble file ", argv[2]) ;
         readfirstscramble(f, pd, scr) ;
         fclose(f) ;
      }
      prunetable pt(pd, maxmem) ;
      processlines2(pd, [&](const puzdef &pd, setval p1sol, const char *p1str) {
                               dophase2(pd, scr, p1sol, pt, p1str); }) ;
   } else if (argc > 2) {
      f = fopen(argv[2], "r") ;
      if (f == 0)
         error("! could not open scramble file ", argv[2]) ;
      processscrambles(f, pd) ;
   }
}
