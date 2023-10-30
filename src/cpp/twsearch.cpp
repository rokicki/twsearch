#include "twsearch.h"
#include "antipode.h"
#include "canon.h"
#include "city.h"
#include "cmdlineops.h"
#include "coset.h"
#include "descsets.h"
#include "filtermoves.h"
#include "findalgo.h"
#include "generatingset.h"
#include "god.h"
#include "index.h"
#include "orderedgs.h"
#include "ordertree.h"
#include "parsemoves.h"
#include "prunetable.h"
#include "puzdef.h"
#include "readksolve.h"
#include "rotations.h"
#include "solve.h"
#include "test.h"
#include "threads.h"
#include "util.h"
#include "workchunks.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <math.h>
#include <sstream>
#include <thread>
#include <vector>
using namespace std;
int checkbeforesolve;
generatingset *gs;
int bestsolve = 1000000;
int optmaxdepth = 0;
int usehashenc;
int dogod, docanon, doalgo, dosolvetest, dotimingtest, douniq, doinv,
    dosolvelines, doorder, doshowmoves, doshowpositions, dogenrand,
    checksolvable, doss, doorderedgs, dosyms, docancelseqs, domergeseqs,
    dounrotateseqs, doshortenseqs, docoset, douniqsymm, dodescsets, doordertree,
    dowrong;
const char *scramblealgo = 0;
const char *legalmovelist = 0;
static int initialized = 0;
int seed = 0;
int forcearray = 0;
void reseteverything() {
  checkbeforesolve = 0;
  optmaxdepth = 0;
  usehashenc = 0;
  scramblealgo = 0;
  legalmovelist = 0;
  seed = 0;
  bestsolve = 1000000;
  forcearray = 0;
  dogod = 0;
  docanon = 0;
  doalgo = 0;
  dosolvetest = 0;
  dotimingtest = 0;
  douniq = 0;
  doinv = 0;
  dosolvelines = 0;
  doorder = 0;
  doshowmoves = 0;
  doshowpositions = 0;
  dogenrand = 0;
  checksolvable = 0;
  doss = 0;
  doorderedgs = 0;
  dosyms = 0;
  docancelseqs = 0;
  domergeseqs = 0;
  dounrotateseqs = 0;
  doshortenseqs = 0;
  docoset = 0;
  douniqsymm = 0;
  dodescsets = 0;
  doordertree = 0;
  dowrong = 0;
// for now, WASM limit is 1GB; normal C++ limit is 8GB
#ifdef WASM
  maxmem = 1LL * 1024LL * 1024LL * 1024LL;
  writeprunetables = 0; // never
#else
  maxmem = 8LL * 1024LL * 1024LL * 1024LL;
  writeprunetables = 1; // auto
#endif
  antipodecount = 20;
  canonmask.clear();
  canonnext.clear();
  ccount = 0;
  canonlim = 0;
  uniqwork.clear();
  uniqseen.clear();
  globalinputmovecount = 0;
  proclim = 1'000'000'000'000'000'000LL;
  compact = 0;
  maxwrong = 0;
  cosetmovelist = 0;
  cosetmoveseq = 0;
  listcosets = 0;
  relaxcosets = 0;
  algostrict = 0;
  cnts.clear();
  symcoordgoal = 20000;
  parts.clear();
  looseper = 0;
  looseiper = 0;
  basebits = 0;
  usehashenc = 0;
  inputbasename = UNKNOWNPUZZLE;
  startprunedepth = 3;
  workerparams.clear();
  dllstates = 0;
  origroup = 0;
  posns.clear();
  movehist.clear();
  nocorners = 0;
  nocenters = 0;
  noedges = 0;
  ignoreori = 0;
  distinguishall = 0;
  omitsets.clear();
  solutionsfound = 0;
  solutionsneeded = 1;
  noearlysolutions = 0;
  phase2 = 0;
  optmindepth = 0;
  onlyimprovements = 0;
  randomstart = 0;
  lastsolution.clear();
  maxdepth = 1000000000;
  didprepass = 0;
  scramblemoves = 1;
#ifdef USE_PTHREADS
  numthreads = min((unsigned int)MAXTHREADS, thread::hardware_concurrency());
#else
  numthreads = 1;
#endif
  requesteduthreading = 4;
  verbose = 1;
  curline.clear();
  start = walltime();
  quarter = 0;
  quiet = 0;
  workchunks.clear();
  workstates.clear();
  workat = 0;
}
void dophase2(const puzdef &pd, setval scr, setval p1sol, prunetable &pt,
              const char *p1str) {
  stacksetval p2(pd);
  if (optmaxdepth == 0)
    optmaxdepth = maxdepth;
  pd.mul(scr, p1sol, p2);
  maxdepth = min(optmaxdepth - globalinputmovecount,
                 bestsolve - globalinputmovecount - 1);
  int r = solve(pd, pt, p2, gs);
  if (r >= 0) {
    cout << "Phase one was " << p1str << endl;
    bestsolve = r + globalinputmovecount;
    cout << "Found a solution totaling " << bestsolve << " moves." << endl;
  }
}
void doinit() {
  if (!initialized) {
// disable saving pruning tables when running under WASM
#ifdef WASM
    writeprunetables = 0; // never
#endif
    init_util();
    init_threads();
    if (seed)
      mysrand(seed);
    else
      mysrand(time(0));
    initialized = 1;
  }
}
/*
 *   Can be called multiple times at the start.
 */
void processargs(int &argc, argvtype &argv) {
  while (argc > 1 && argv[1][0] == '-') {
    argc--;
    argv++;
    if (argv[0][1] == '-') {
      if (strcmp(argv[0], "--moves") == 0) {
        legalmovelist = argv[1];
        argc--;
        argv++;
      } else if (strcmp(argv[0], "--showmoves") == 0) {
        doshowmoves++;
      } else if (strcmp(argv[0], "--showpositions") == 0) {
        doshowpositions++;
      } else if (strcmp(argv[0], "--newcanon") == 0) {
        ccount = atol(argv[1]);
        argc--;
        argv++;
      } else if (strcmp(argv[0], "--nocorners") == 0) {
        nocorners++;
      } else if (strcmp(argv[0], "--nocenters") == 0) {
        nocenters++;
      } else if (strcmp(argv[0], "--noorientation") == 0) {
        ignoreori = 1;
      } else if (strcmp(argv[0], "--omit") == 0) {
        omitsets.insert(argv[1]);
        argc--;
        argv++;
      } else if (strcmp(argv[0], "--distinguishall") == 0) {
        distinguishall = 1;
      } else if (strcmp(argv[0], "--noearlysolutions") == 0) {
        noearlysolutions = 1;
      } else if (strcmp(argv[0], "--checkbeforesolve") == 0) {
        checkbeforesolve = 1;
      } else if (strcmp(argv[0], "--orientationgroup") == 0) {
        origroup = atol(argv[1]);
        argc--;
        argv++;
      } else if (strcmp(argv[0], "--noedges") == 0) {
        noedges++;
      } else if (strcmp(argv[0], "--scramblealg") == 0) {
        scramblealgo = argv[1];
        argc--;
        argv++;
      } else if (strcmp(argv[0], "--schreiersims") == 0) {
        doss = 1;
      } else if (strcmp(argv[0], "--orderedgs") == 0) {
        doorderedgs = 1;
      } else if (strcmp(argv[0], "--showsymmetry") == 0) {
        dosyms = 1;
      } else if (strcmp(argv[0], "--microthreads") == 0) {
        requesteduthreading = atol(argv[1]);
        argc--;
        argv++;
      } else if (strcmp(argv[0], "--nowrite") == 0) {
        writeprunetables = 0; // never
      } else if (strcmp(argv[0], "--writeprunetables") == 0) {
        const char *arg = argv[1];
        argc--;
        argv++;
        if (strcmp(arg, "never") == 0)
          writeprunetables = 0;
        else if (strcmp(arg, "auto") == 0)
          writeprunetables = 1;
        else if (strcmp(arg, "always") == 0)
          writeprunetables = 2;
        else
          error(
              "! the --writeprunetables option expects always, auto, or never");
      } else if (strcmp(argv[0], "--cachedir") == 0) {
        user_option_cache_dir = argv[1];
        argc--;
        argv++;
      } else if (strcmp(argv[0], "--quiet") == 0) {
        quiet++;
        verbose = 0;
      } else if (strcmp(argv[0], "--mergeseqs") == 0) {
        domergeseqs++;
      } else if (strcmp(argv[0], "--unrotateseqs") == 0) {
        dounrotateseqs++;
      } else if (strcmp(argv[0], "--shortenseqs") == 0) {
        doshortenseqs++;
      } else if (strcmp(argv[0], "--cancelseqs") == 0) {
        docancelseqs++;
      } else if (strcmp(argv[0], "--randomstart") == 0) {
        randomstart++;
      } else if (strcmp(argv[0], "--startprunedepth") == 0) {
        startprunedepth = atol(argv[1]);
        argc--;
        argv++;
      } else if (strcmp(argv[0], "--mindepth") == 0) {
        optmindepth = atol(argv[1]);
        argc--;
        argv++;
      } else if (strcmp(argv[0], "--maxdepth") == 0) {
        maxdepth = atol(argv[1]);
        argc--;
        argv++;
      } else if (strcmp(argv[0], "--coset") == 0) {
        cosetmovelist = argv[1];
        cosetmoveseq = argv[2];
        docoset++;
        argc -= 2;
        argv += 2;
      } else if (strcmp(argv[0], "--listcosets") == 0) {
        listcosets++;
      } else if (strcmp(argv[0], "--relaxcosets") == 0) {
        relaxcosets++;
      } else if (strcmp(argv[0], "--compact") == 0) {
        compact++;
      } else if (strcmp(argv[0], "--describesets") == 0) {
        dodescsets++;
      } else if (strcmp(argv[0], "--ordertree") == 0) {
        doordertree++;
      } else if (strcmp(argv[0], "--maxwrong") == 0) {
        dowrong++;
        maxwrong = atol(argv[1]);
        argc--;
        argv++;
      } else {
        error("! Argument not understood ", argv[0]);
      }
    } else {
      switch (argv[0][1]) {
      case 'q':
        quarter++;
        break;
      case 'v':
        verbose++;
        if (argv[0][2] != 0)
          verbose = argv[0][2] - '0';
        break;
      case 'm':
      case 'd':
        maxdepth = atol(argv[1]);
        argc--;
        argv++;
        break;
      case 'r':
        dogenrand = 1;
        if (argv[0][2] != 0)
          dogenrand = atol(argv[0] + 2);
        break;
      case 'R':
        seed = atol(argv[1]);
        argc--;
        argv++;
        break;
      case 'H':
        usehashenc++;
        break;
      case 'M':
        maxmem = 1048576 * atoll(argv[1]);
        argc--;
        argv++;
        break;
      case 'y':
        symcoordgoal = atoll(argv[1]);
        if (symcoordgoal <= 0)
          symcoordgoal = 1;
        argc--;
        argv++;
        break;
      case 'c':
        solutionsneeded = atoll(argv[1]);
        argc--;
        argv++;
        break;
      case 'g':
        dogod++;
        break;
      case 'o':
        doorder++;
        break;
      case 'U':
        douniqsymm++;
        if (argv[0][2] >= '0')
          proclim = atoll(argv[0] + 2);
        break;
      case 'u':
        douniq++;
        if (argv[0][2] >= '0')
          proclim = atoll(argv[0] + 2);
        break;
      case 'i':
        doinv++;
        break;
      case 's':
        dosolvelines++;
        if (argv[0][2] == 'i')
          onlyimprovements = 1;
        break;
      case 'C':
        docanon++;
        if (argv[0][2] >= '0') {
          canonlim = atoll(argv[0] + 2);
        }
        break;
      case 'F':
        forcearray++;
        break;
      case 'a':
        antipodecount = atoll(argv[1]);
        argc--;
        argv++;
        break;
      case 'A':
        doalgo = -1;
        for (int pp = 2; argv[0][pp]; pp++)
          if (argv[0][pp] == '1')
            doalgo = 1;
          else if (argv[0][pp] == '2')
            doalgo = 2;
          else if (argv[0][pp] == '3')
            doalgo = 3;
          else if (argv[0][pp] == 's')
            algostrict = 1;
        break;
      case 'T':
        dotimingtest++;
        break;
      case 'S':
        dosolvetest++;
        if (argv[0][2])
          scramblemoves = atol(argv[0] + 2);
        break;
      case 't':
        numthreads = atol(argv[1]);
        if (numthreads > MAXTHREADS)
          error("Numthreads cannot be more than ", to_string(MAXTHREADS));
        argc--;
        argv++;
        break;
      case '2':
        phase2 = 1;
        break;
      default:
        error("! did not argument ", argv[0]);
      }
    }
  }
}
puzdef makepuzdef(istream *f) {
  doinit();
  puzdef pd = readdef(f);
  addmovepowers(pd);
  if (legalmovelist)
    filtermovelist(pd, legalmovelist);
  if (nocorners)
    pd.addoptionssum("nocorners");
  if (nocenters)
    pd.addoptionssum("nocenters");
  if (noedges)
    pd.addoptionssum("noedges");
  if (ignoreori)
    pd.addoptionssum("noorientation");
  if (omitsets.size()) {
    pd.addoptionssum("omit");
    for (auto s : omitsets)
      pd.addoptionssum(s.c_str());
  }
  if (distinguishall)
    pd.addoptionssum("distinguishall");
  if (doss || checkbeforesolve) {
    if (!doss && !pd.uniq)
      warn("Ignoring --checkbeforesolve due to identical pieces");
    else if (!doss && pd.wildo)
      warn("Ignoring --checkbeforesolve due to orientation wildcards");
    else if (!doss && pd.haveillegal)
      warn("Ignoring --checkbeforesolve due to illegal positions");
    else
      gs = new generatingset(pd);
  }
  if (pd.rotations.size())
    calcrotations(pd);
  calculatesizes(pd);
  calclooseper(pd);
  if (ccount == 0)
    makecanonstates(pd);
  else
    makecanonstates2(pd);
  if (quiet == 0)
    cout << "Calculated canonical states in " << duration() << endl << flush;
  showcanon(pd, docanon);
  //   gensymm(pd) ;
  return pd;
}
puzdef makepuzdef(string s) {
  stringstream is(s);
  return makepuzdef(&is);
}
int main_search(const char *def_file, const char *scramble_file) {
  if (scramblealgo && doshortenseqs)
    error("! --shortenseqs takes input from stdin, not from a command line "
          "algorithm.");
  ifstream f;
  f.open(def_file, ifstream::in);
  if (f.fail())
    error("! could not open file ", def_file);
  int sawdot = 0;
  inputbasename.clear();
  for (int i = 0; def_file[i]; i++) {
    if (def_file[i] == '.')
      sawdot = 1;
    else if (def_file[i] == '/' || def_file[i] == '\\') {
      sawdot = 0;
      inputbasename.clear();
    } else if (!sawdot)
      inputbasename.push_back(def_file[i]);
  }
  puzdef pd = makepuzdef(&f);
  if (doorderedgs)
    runorderedgs(pd);
  if (dodescsets) {
    descsets(pd);
  }
  if (doordertree) {
    ordertree(pd);
  }
  if (dogenrand) {
    for (int i = 0; i < dogenrand; i++)
      showrandompos(pd);
    return 0;
  }
  if (dogod) {
    int statesfit2 = pd.logstates <= 50 && ((ll)(pd.llstates >> 2)) <= maxmem;
    int statesfitsa =
        forcearray ||
        (pd.logstates <= 50 &&
         ((ll)(pd.llstates * sizeof(loosetype) * looseper) <= maxmem));
    if (!forcearray && statesfit2 && pd.canpackdense()) {
      cout << "Using twobit arrays." << endl;
      dotwobitgod2(pd);
    } else if (statesfitsa) {
      if (pd.rotgroup.size()) {
        cout << "Using sorting bfs symm and arrays." << endl;
        doarraygodsymm(pd);
      } else {
        cout << "Using sorting bfs and arrays." << endl;
        doarraygod(pd);
      }
    } else {
      cout << "Using canonical sequences and arrays." << endl;
      doarraygod2(pd);
    }
  }
  if (doalgo)
    findalgos(pd, doalgo);
  if (dosolvetest)
    solvetest(pd, gs);
  if (dotimingtest)
    timingtest(pd);
  if (!phase2 && scramblealgo)
    solvecmdline(pd, scramblealgo, gs);
  if (douniq)
    processlines(pd, uniqit);
  if (dowrong)
    processlines(pd, wrongit);
  if (douniqsymm)
    processlines2(pd, uniqitsymm);
  if (doinv)
    processlines3(pd, invertit);
  if (domergeseqs)
    processlines3(pd, mergeit);
  if (dounrotateseqs)
    processlines4(pd, unrotateit);
  if (doshortenseqs)
    processlines3(pd, shortenit);
  if (docancelseqs)
    processlines3(pd, cancelit);
  if (dosyms)
    processlines(pd, symsit);
  if (doorder)
    processlines2(pd, orderit);
  if (doshowmoves)
    processlines2(pd, emitmove);
  if (doshowpositions)
    processlines(pd, emitposition);
  if (dosolvelines) {
    prunetable pt(pd, maxmem);
    string emptys;
    processlines(pd, [&](const puzdef &pd, setval p, const char *) {
      solveit(pd, pt, emptys, p, gs);
    });
  }
  if (docoset) {
    runcoset(pd);
  }
  if (phase2) {
    if (scramble_file == NULL && !scramblealgo)
      error("! need a scramble file for phase 2");
    stacksetval scr(pd);
    if (scramblealgo) {
      pd.assignpos(scr, pd.solved);
      vector<allocsetval> movelist = parsemovelist_generously(pd, scramblealgo);
      for (int i = 0; i < (int)movelist.size(); i++)
        domove(pd, scr, movelist[i]);
    } else {
      ifstream scrambles;
      scrambles.open(scramble_file, ifstream::in);
      if (scrambles.fail())
        error("! could not open scramble file ", scramble_file);
      readfirstscramble(&scrambles, pd, scr);
      scrambles.close();
    }
    prunetable pt(pd, maxmem);
    processlines2(pd, [&](const puzdef &pd, setval p1sol, const char *p1str) {
      dophase2(pd, scr, p1sol, pt, p1str);
    });
  } else if (scramble_file != NULL) {
    ifstream scrambles;
    scrambles.open(scramble_file, ifstream::in);
    if (scrambles.fail())
      error("! could not open scramble file ", scramble_file);
    processscrambles(&scrambles, pd, gs);
    scrambles.close();
  }
  cout << "Twsearch finished." << endl;
  return 0;
}

#ifndef ASLIBRARY
#define STR2(x) #x
#define STRINGIZE(x) STR2(x)
int main(int argc, const char **argv) {
  reseteverything();
  int orig_argc = argc;
  const char **orig_argv = argv;
  processargs(argc, argv);
  if (quiet == 0) {
    cout << "# This is twsearch "
         << STRINGIZE(TWSEARCH_VERSION) << " (C) 2022 Tomas Rokicki." << endl;
    cout << "#";
    for (int i = 0; i < orig_argc; i++)
      cout << " " << orig_argv[i];
    cout << endl << flush;
  }

  if (argc <= 1)
    error("! please provide a twsearch file name on the command line");

  const char *def_file = argv[1];
  const char *scramble_file = NULL;
  if (argc > 2) {
    scramble_file = argv[2];
  }

  return main_search(def_file, scramble_file);
}
#endif
