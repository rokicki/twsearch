#include "twsearch.h"
#include "canon.h"
#include "city.h"
#include "cmds.h"
#include "filtermoves.h"
#include "generatingset.h"
#include "index.h"
#include "parsemoves.h"
#include "prunetable.h"
#include "puzdef.h"
#include "readksolve.h"
#include "rotations.h"
#include "solve.h"
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
int optmaxdepth = 0;
int usehashenc;
cmd *cmdhead, *requestedcmd;
const char *scramblealgo = 0;
const char *legalmovelist = 0;
static int initialized = 0;
int seed = 0;
void reseteverything() {
  checkbeforesolve = 0;
  optmaxdepth = 0;
  usehashenc = 0;
  scramblealgo = 0;
  legalmovelist = 0;
  seed = 0;
// for now, WASM limit is 1GB; normal C++ limit is 8GB
#ifdef WASM
  maxmem = 1LL * 1024LL * 1024LL * 1024LL;
  writeprunetables = 0; // never
#else
  maxmem = 8LL * 1024LL * 1024LL * 1024LL;
  writeprunetables = 1; // auto
#endif
  ccount = 0;
  canonlim = 0;
  inputbasename = UNKNOWNPUZZLE;
  startprunedepth = 3;
  origroup = 0;
  nocorners = 0;
  nocenters = 0;
  noedges = 0;
  ignoreori = 0;
  distinguishall = 0;
  omitsets.clear();
  solutionsfound = 0;
  solutionsneeded = 1;
  noearlysolutions = 0;
  optmindepth = 0;
  onlyimprovements = 0;
  randomstart = 0;
  maxdepth = 1000000000;
  didprepass = 0;
#ifdef USE_PTHREADS
  numthreads = min((unsigned int)MAXTHREADS, thread::hardware_concurrency());
#else
  numthreads = 1;
#endif
  requesteduthreading = 4;
  verbose = 1;
  start = walltime();
  quarter = 0;
  quiet = 0;
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
void processargs(int &argc, argvtype &argv, int includecmds) {
  while (argc > 1 && argv[1][0] == '-') {
    argc--;
    argv++;
    if (strcmp(argv[0], "--moves") == 0) {
      legalmovelist = argv[1];
      argc--;
      argv++;
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
        error("! the --writeprunetables option expects always, auto, or never");
    } else if (strcmp(argv[0], "--cachedir") == 0) {
      user_option_cache_dir = argv[1];
      argc--;
      argv++;
    } else if (strcmp(argv[0], "--quiet") == 0) {
      quiet++;
      verbose = 0;
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
    } else if (strcmp(argv[0], "-q") == 0) {
      quarter++;
    } else if (strcmp(argv[0], "-v") == 0) {
      verbose++;
      if (argv[0][2] != 0)
        verbose = argv[0][2] - '0';
    } else if (strcmp(argv[0], "-m") == 0 || strcmp(argv[0], "-d") == 0) {
      maxdepth = atol(argv[1]);
      argc--;
      argv++;
    } else if (strcmp(argv[0], "-R") == 0) {
      seed = atol(argv[1]);
      argc--;
      argv++;
    } else if (strcmp(argv[0], "-H") == 0) {
      usehashenc++;
    } else if (strcmp(argv[0], "-M") == 0) {
      maxmem = 1048576 * atoll(argv[1]);
      argc--;
      argv++;
    } else if (strcmp(argv[0], "-c") == 0) {
      solutionsneeded = atoll(argv[1]);
      argc--;
      argv++;
    } else if (strcmp(argv[0], "-t") == 0) {
      numthreads = atol(argv[1]);
      if (numthreads > MAXTHREADS)
        error("Numthreads cannot be more than ", to_string(MAXTHREADS));
      argc--;
      argv++;
    } else {
      int found = 0;
      for (auto p = cmdhead; includecmds && p; p = p->next) {
        if ((p->shortoption && strncmp(argv[0], p->shortoption, 2) == 0) ||
            (p->longoption && strcmp(argv[0], p->longoption) == 0)) {
          p->parse_args(&argc, &argv);
          if (p->ismaincmd()) {
            if (requestedcmd != 0)
              error("! can only do one thing at a time");
            requestedcmd = p;
          }
          found = 1;
          break;
        }
      }
      if (!found)
        error("! Argument not understood ", argv[0]);
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
  if (checkbeforesolve) {
    if (!pd.uniq)
      warn("Ignoring --checkbeforesolve due to identical pieces");
    else if (pd.wildo)
      warn("Ignoring --checkbeforesolve due to orientation wildcards");
    else if (pd.haveillegal)
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
  showcanon(pd, 0);
  //   gensymm(pd) ;
  return pd;
}
puzdef makepuzdef(string s) {
  stringstream is(s);
  return makepuzdef(&is);
}

int getcompactval(int &at, const string &s) {
  if (at < 0 || at >= (int)s.size())
    error("! out of bounds while reading compact");
  char c = s[at++];
  if ('0' <= c && c <= '9')
    return c - '0';
  if ('A' <= c && c <= 'Z')
    return c - 'A' + 10;
  if ('a' <= c && c <= 'z')
    return c - 'a' + 36;
  error("! bad character in compact format");
  return -1;
}

void readposition(puzdef &pd, setval &p1, string crep) {
  int at = 0;
  for (int i = 0; i < (int)pd.setdefs.size(); i++) {
    setdef &sd = pd.setdefs[i];
    int n = sd.size;
    int ss = n * sd.omod;
    int off = sd.off;
    for (int j = 0; j < n; j++) {
      int v = 0;
      if (ss <= 62) {
        v = getcompactval(at, crep);
      } else if (ss <= 62 * 62) {
        v = getcompactval(at, crep);
        v = v * 62 + getcompactval(at, crep);
      } else {
        error("! can't read compact format for this puzdef");
      }
      if (v < 0 || v >= ss)
        error("! bad value in compact format");
      p1.dat[off + j] = v / sd.omod;
      p1.dat[off + j + n] = v % sd.omod;
    }
  }
  if (at != (int)crep.size())
    error("! extra input in compact format");
}

void processscrambles(istream *f, puzdef &pd, prunetable &pt,
                      generatingset *gs) {
  string scramblename;
  ull checksum = 0;
  stacksetval p1(pd);
  while (1) {
    vector<string> toks = getline(f, checksum);
    if (toks.size() == 0)
      break;
    if (toks[0] == "Scramble" || toks[0] == "ScrambleState" ||
        toks[0] == "StartState") {
      expect(toks, 2);
      scramblename = toks[1];
      allocsetval p =
          readposition(pd, 'S', f, checksum,
                       toks[0] == "ScrambleState" || toks[0] == "StartState");
      solveit(pd, pt, scramblename, p, gs);
    } else if (toks[0] == "ScrambleAlg") {
      expect(toks, 2);
      scramblename = toks[1];
      pd.assignpos(p1, pd.solved);
      while (1) {
        toks = getline(f, checksum);
        if (toks.size() == 0)
          error("! early end of line while reading ScrambleAlg");
        if (toks[0] == "End")
          break;
        for (int i = 0; i < (int)toks.size(); i++)
          domove(pd, p1, findmove_generously(pd, toks[i]));
      }
      solveit(pd, pt, scramblename, p1, gs);
    } else if (toks[0] == "CPOS") {
      expect(toks, 2);
      scramblename = "noname";
      readposition(pd, p1, toks[1]);
      solveit(pd, pt, scramblename, p1, gs);
    } else {
      error("! unsupported command in scramble file");
    }
  }
}

void processscrambles(istream *f, puzdef &pd, generatingset *gs) {
  prunetable pt(pd, maxmem);
  processscrambles(f, pd, pt, gs);
}

int main_search(const char *def_file, const char *scramble_file) {
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
  if (requestedcmd) {
    requestedcmd->docommand(pd);
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
  processargs(argc, argv, 1);
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
