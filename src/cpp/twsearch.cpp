#include "twsearch.h"
#include "canon.h"
#include "city.h"
#include "cmdlineops.h"
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
#include "subgroup.h"
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
int optmaxdepth = 0;
int verbosecanon;
cmd *requestedcmd, *cmdhead;
const char *legalmovelist = 0;
static int initialized = 0;
int seed = 0;
void reseteverything() {
  checkbeforesolve = 0;
  optmaxdepth = 0;
  usehashenc = 0;
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
  verbosecanon = 0;
  ignoreori = 0;
  distinguishall = 0;
  omitsets.clear();
  omitoris.clear();
  omitperms.clear();
  setsmustexist.clear();
  solutionsfound = 0;
  solutionsneeded = 1;
  noearlysolutions = 0;
  optmindepth = 0;
  onlyimprovements = 0;
  randomstart = 0;
  alloptimal = 0;
  disablesymmetry = 0;
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
#ifdef _WIN64
      mysrand(GetTickCount());
#else
      mysrand(time(0));
#endif
    initialized = 1;
  }
}
static stringopt stringopts[] = {
    {"--moves", "moves  Restrict search to the given moves.", &legalmovelist},
    {"--cachedir",
     "dirname  Use the specified directory to cache pruning tables.",
     &user_option_cache_dir},
};
static boolopt boolopts[] = {
    {"--nocorners", "Omit any puzzle sets with recognizable corner names.",
     &nocorners},
    {"--nocenters", "Omit any puzzle sets with recognizable center names.",
     &nocenters},
    {"--noedges", "Omit any puzzle sets with recognizable edge names.",
     &noedges},
    {"--noorientation", "Ignore orientations for all sets.", &ignoreori},
    {"--distinguishall",
     "Override distinguishable pieces (use the superpuzzle).", &distinguishall},
    {"--noearlysolutions",
     "Emit any solutions whose prefix is also a solution.", &noearlysolutions},
    {"--checkbeforesolve",
     "Check each position for solvability using generating\n"
     "set before attempting to solve.",
     &checkbeforesolve},
    {"--randomstart", "Randomize move order when solving.", &randomstart},
    {"-q", "Use only minimal (quarter) turns.", &quarter},
    {"-H", "Use 128-bit hash instead of full state for God's number searches.",
     &usehashenc},
    {"--alloptimal",
     "Find all optimal solutions.  If puzzle has rotations\n"
     "and is reduced by symmetry, the set of solutions will also be\n"
     "reduced by that symmetry.",
     &alloptimal},
    {"--nosymmetry", "Disable all symmetry reductions.", &disablesymmetry},
};
static intopt intopts[] = {
    {"--newcanon",
     "num  Use search-based canonical sequences to the given depth.", &ccount,
     0, 100},
    {"-t", "num  Use this many threads.", &numthreads, 1, MAXTHREADS},
    {"--microthreads", "num  Use this many microthreads on each thread.",
     &requesteduthreading, 1, MAXMICROTHREADING},
    {"--orientationgroup",
     "num  Treat adjacent piece groups of this size as\n"
     "orientations.",
     &origroup, 1, 255},
    {"--startprunedepth",
     "num  Initial depth for pruning tables (default is 3).", &startprunedepth,
     0, 100},
    {"--mindepth", "num  Minimum depth for searches.", &optmindepth, 0, 1000},
    {"--maxdepth", "num  Maximum depth for searches.", &maxdepth, 0, 1000},
    {"-R", "num  Seed for random number generator.", &seed, -2000000000,
     2000000000},
};
static llopt solcountopt(
    "-c",
    "num  Number of solutions to generate.  If puzzle has rotations\n"
    "and is reduced by symmetry, the set of solutions will also be\n"
    "reduced by that symmetry.",
    &solutionsneeded);
/*
 *   Can be called multiple times at the start.
 */
void processargs(int &argc, argvtype &argv, int includecmds) {
// If we are compiling the Rust bridge, we want to support the benchmark
// command, which requires the test.cpp compilation unit; to ensure this is
// part of the executable we need to reference it here.  But we don't
// bring it into the WASM.
#ifdef ASLIBRARY
#ifndef WASM
  ensure_test_is_linked();
#endif
#endif
  while (argc > 1 && argv[1][0] == '-') {
    argc--;
    argv++;
    int found = 0;
    for (auto p = cmdhead; p; p = p->next) {
      // we permit additional suffixes on two-letter options, like -v0
      if ((includecmds || !p->ismaincmd()) &&
          0 == (p->option[2] ? strcmp(argv[0], p->option)
                             : strncmp(argv[0], p->option, 2))) {
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
    if (!found) {
      printhelp();
      error("! Argument not understood ", argv[0]);
    }
  }
}
puzdef makepuzdef(istream *f) {
  doinit();
  puzdef pd = readdef(f);
  filtermovelist(pd, legalmovelist);
  if (subgroupmovelist != 0)
    runsubgroup(pd);
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
  if (omitoris.size()) {
    pd.addoptionssum("omitoris");
    for (auto s : omitoris)
      pd.addoptionssum(s.c_str());
  }
  if (omitperms.size()) {
    pd.addoptionssum("omitperms");
    for (auto s : omitperms)
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
  if (pd.baserotations.size())
    calcrotations(pd);
  calculatesizes(pd);
  calclooseper(pd);
  if (ccount == 0)
    makecanonstates(pd);
  else
    makecanonstates2(pd);
  if (quiet == 0)
    cout << "Calculated canonical states in " << duration() << endl << flush;
  showcanon(pd, verbosecanon);
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
      // this call is to readksolve.cpp's readposition, not the one above
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
    error("! could not open definition file ", def_file);
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
  if (verbose)
    cout << "Twsearch finished." << endl;
  return 0;
}

static struct cmdcanoncmd : cmd {
  cmdcanoncmd()
      : cmd("-C",
            "Show canonical sequence counts.  The option can be followed\n"
            "immediately by a number of levels (e.g., -C20).") {}
  virtual void parse_args(int *, const char ***argv) {
    const char *p = **argv + 2;
    if (*p)
      canonlim = atol(p);
    else
      canonlim = 100;
    verbosecanon = 1;
  }
  void docommand(puzdef &) {}
} registercanoncmd;

static struct cmdlinescramblecmd : cmd {
  cmdlinescramblecmd()
      : cmd("--scramblealg", "moveseq  Give a scramble as a sequence of moves "
                             "on the\n"
                             "command line.") {}
  virtual void parse_args(int *argc, const char ***argv) {
    (*argc)--;
    (*argv)++;
    scramblealgo = **argv;
  }
  virtual void docommand(puzdef &pd) { solvecmdline(pd, scramblealgo, gs); }
  const char *scramblealgo = 0;
} registercmdlinescramble;

static struct omitopt : specialopt {
  omitopt()
      : specialopt(
            "--omit",
            "setname  Omit the following set name from the puzzle.  You can "
            "provide\n"
            "as many separate omit options, each with a separate set name, as "
            "you want.") {}
  virtual void parse_args(int *argc, const char ***argv) {
    (*argc)--;
    (*argv)++;
    omitsets.insert(**argv);
    setsmustexist.insert(**argv);
  }
} registeromitopt;

static struct omitpermsopt : specialopt {
  omitpermsopt()
      : specialopt(
            "--omitperms",
            "setname  Omit the permutations for the following set name from "
            "the puzzle.\nYou can provide as many separate omitperms options, "
            " each with a separate\nset name, as you want.") {}
  virtual void parse_args(int *argc, const char ***argv) {
    (*argc)--;
    (*argv)++;
    omitperms.insert(**argv);
    setsmustexist.insert(**argv);
  }
} registeromitpermsopt;

static struct omitorisopt : specialopt {
  omitorisopt()
      : specialopt(
            "--omitoris",
            "setname  Omit the permutations for the following set name from "
            "the puzzle.\nYou can provide as many separate omitoris options, "
            " each with a separate\nset name, as you want.") {}
  virtual void parse_args(int *argc, const char ***argv) {
    (*argc)--;
    (*argv)++;
    omitoris.insert(**argv);
    setsmustexist.insert(**argv);
  }
} registeromitorisopt;

static struct nowriteopt : specialopt {
  nowriteopt() : specialopt("--nowrite", "Do not write pruning tables.") {}
  virtual void parse_args(int *, const char ***) { writeprunetables = 0; }
} registernowriteopt;

static struct writepruneopt : specialopt {
  writepruneopt()
      : specialopt("--writeprunetables",
                   "never|auto|always  Specify when or if pruning tables\n"
                   "should be written  The default is auto, which writes only "
                   "when the program\n"
                   "thinks the pruning table will be faster to read than to "
                   "regenerate.") {}
  virtual void parse_args(int *argc, const char ***argv) {
    (*argc)--;
    (*argv)++;
    const char *p = **argv;
    if (strcmp(p, "never") == 0)
      writeprunetables = 0;
    else if (strcmp(p, "auto") == 0)
      writeprunetables = 1;
    else if (strcmp(p, "always") == 0)
      writeprunetables = 2;
    else
      error("! the --writeprunetables option should be followed by never, "
            "auto, or always.");
  }
} registerwritepruneopt;

static struct quietopt : specialopt {
  quietopt() : specialopt("--quiet", "Eliminate extraneous output.") {}
  virtual void parse_args(int *, const char ***) {
    quiet++;
    verbose = 0;
  }
} registerquietopt;

static struct verboseopt : specialopt {
  verboseopt()
      : specialopt("-v",
                   "Increase verbosity level.  If followed immediately by a "
                   "digit, set\n"
                   "that verbosity level.") {}
  virtual void parse_args(int *, const char ***argv) {
    verbose++;
    const char *p = **argv + 2;
    if (*p) {
      if ('0' <= *p && *p <= '9')
        verbose = *p - '0';
      else
        error("The -v option should be followed by nothing or a single digit.");
    }
  }
} registerverboseopt;

static struct memopt : specialopt {
  memopt() : specialopt("-M", "num  Set maximum memory use in megabytes.") {}
  virtual void parse_args(int *argc, const char ***argv) {
    (*argc)--;
    (*argv)++;
    maxmem = 1048576LL * atol(**argv);
  }
} registermemopt;

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

  if (argc <= 1) {
    printhelp();
    error("! please provide a twsearch file name on the command line");
  }

  const char *def_file = argv[1];
  const char *scramble_file = NULL;
  if (argc > 2) {
    scramble_file = argv[2];
  }

  return main_search(def_file, scramble_file);
}
#endif
