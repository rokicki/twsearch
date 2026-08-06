#include "jit.h"
#include "util.h"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <unistd.h>
int enablejit;
const char *jitcc;
/*
 *   Code generation.  For a single rotation m, rotconjugate/rotconjugatecmp
 *   (see puzdef.h) walk every setdef and, for every element j in it, do
 *
 *      dp[j]   = ap[bp[cp[j]]]                                    (perm)
 *      dp[j+n] = moda[ap[bp[cp[j]]+n] + moda[bp[cp[j]+n] + cp[j+n]]]  (ori)
 *
 *   where ap = rotinvmap[m].dat, cp = rotgroup[m].pos.dat, and bp/dp are
 *   the actual (runtime) position being reduced and the candidate result.
 *   For a *specific* puzzle, ap and cp never change after rotations are
 *   calculated --- only bp/dp (the puzzle state) are truly variable at
 *   search time.  So for a fixed m, this whole computation is a function
 *   of bp alone: every cp[j] collapses to a literal integer, every ap[]
 *   lookup can be baked in as a static table, and the setdef/j loop
 *   structure can be fully unrolled into straight-line code.  That's what
 *   this file generates: for every rotation m, a conj<m>() (equivalent to
 *   rotconjugate) and a conjcmp<m>() (equivalent to rotconjugatecmp),
 *   compiled by the system C compiler and dlopen'd back in.
 *
 *   Concretely, for setdef i (offset off, size n) and output slot j:
 *     gidx  = off + rotgroup[m].pos.dat[off+j]     -- both terms constant
 *   so `bp[cp[j]]` is just `bp[gidx]`, a single fixed-offset load (instead
 *   of the two dependent loads --- one to fetch cp[j], one to use it ---
 *   the interpreted version needs).  The matching orientation delta
 *   `cp[j+n]` (= rotgroup[m].pos.dat[off+n+j]) is likewise a compile-time
 *   constant.  Only ap[] (rotinvmap[m].dat) and moda[] (gmoda[omod]) stay
 *   as genuine lookup tables, since they're indexed by a value read from
 *   bp at run time.
 */
static string gensource(const puzdef &pd) {
  ostringstream o;
  o << "typedef unsigned char uc;\n";
  int nrot = (int)pd.rotgroup.size();
  int totsize = pd.totsize;
  for (int m = 0; m < nrot; m++) {
    o << "static const uc AP" << m << "[" << totsize << "] = {";
    const uchar *ap = pd.rotinvmap[m].dat;
    for (int k = 0; k < totsize; k++) {
      if (k)
        o << ",";
      o << (int)ap[k];
    }
    o << "};\n";
  }
  set<int> omods;
  for (auto &sd : pd.setdefs)
    if (sd.omod > 1)
      omods.insert(sd.omod);
  for (int om : omods) {
    uchar *moda = gmoda[om];
    o << "static const uc MODA" << om << "[" << 4 * om << "] = {";
    for (int k = 0; k < 4 * om; k++) {
      if (k)
        o << ",";
      o << (int)moda[k];
    }
    o << "};\n";
  }
  for (int m = 0; m < nrot; m++) {
    const uchar *cp = pd.rotgroup[m].pos.dat;
    o << "void conj" << m << "(const uc *bp, uc *dp) {\n";
    for (auto &sd : pd.setdefs) {
      int n = sd.size, off = sd.off;
      for (int j = 0; j < n; j++) {
        int gidx = off + cp[off + j];
        int didx = off + j;
        o << "  dp[" << didx << "] = AP" << m << "[" << off << "+bp[" << gidx
          << "]];\n";
        if (sd.omod > 1) {
          int delta = cp[off + n + j];
          o << "  dp[" << (didx + n) << "] = MODA" << (int)sd.omod << "[AP" << m
            << "[" << (off + n) << "+bp[" << gidx << "]]+MODA" << (int)sd.omod
            << "[bp[" << (gidx + n) << "]+" << delta << "]];\n";
        } else {
          o << "  dp[" << (didx + n) << "] = 0;\n";
        }
      }
    }
    o << "}\n";
    o << "int conjcmp" << m << "(const uc *bp, uc *dp) {\n";
    o << "  int r = 0;\n";
    for (auto &sd : pd.setdefs) {
      int n = sd.size, off = sd.off;
      for (int j = 0; j < n; j++) {
        int gidx = off + cp[off + j];
        int didx = off + j;
        o << "  { uc nv = AP" << m << "[" << off << "+bp[" << gidx << "]];\n";
        o << "    if (r > 0) dp[" << didx << "] = nv;\n";
        o << "    else if (nv > dp[" << didx << "]) return 1;\n";
        o << "    else if (nv < dp[" << didx << "]) { r = 1; dp[" << didx
          << "] = nv; } }\n";
      }
      for (int j = 0; j < n; j++) {
        int didx = off + n + j;
        if (sd.omod > 1) {
          int gidx = off + cp[off + j];
          int delta = cp[off + n + j];
          o << "  { uc nv = MODA" << (int)sd.omod << "[AP" << m << "["
            << (off + n) << "+bp[" << gidx << "]]+MODA" << (int)sd.omod
            << "[bp[" << (gidx + n) << "]+" << delta << "]];\n";
          o << "    if (r > 0) dp[" << didx << "] = nv;\n";
          o << "    else if (nv > dp[" << didx << "]) return 1;\n";
          o << "    else if (nv < dp[" << didx << "]) { r = 1; dp[" << didx
            << "] = nv; } }\n";
        } else {
          // nv is always 0 here (omod==1), and dp[] is unsigned, so
          // "nv > dp[didx]" can never trigger; fold that away.
          o << "  if (r > 0) dp[" << didx << "] = 0;\n";
          o << "  else if (dp[" << didx << "] != 0) { r = 1; dp[" << didx
            << "] = 0; }\n";
        }
      }
    }
    o << "  return -r;\n}\n";
  }
  o << "void (*twsearch_jit_conjtable[])(const uc*, uc*) = {";
  for (int m = 0; m < nrot; m++) {
    if (m)
      o << ",";
    o << "conj" << m;
  }
  o << "};\n";
  o << "int (*twsearch_jit_conjcmptable[])(const uc*, uc*) = {";
  for (int m = 0; m < nrot; m++) {
    if (m)
      o << ",";
    o << "conjcmp" << m;
  }
  o << "};\n";
  return o.str();
}
// Try each of these, in order, as a C compiler; the generated file is
// plain C, and passing -x c makes the language choice explicit regardless
// of the file's (extensionless) name, so a C++-flavored CXX still works.
static bool trycompile(const string &cc, const string &srcpath,
                       const string &sopath) {
  string cmd = cc + " -x c -O2 -shared -fPIC -o " + sopath + " " + srcpath +
               " > " + srcpath + ".log 2>&1";
  return system(cmd.c_str()) == 0;
}
// Print the given file's contents under a labeled banner, so a failure is
// actually diagnosable instead of just "JIT didn't work" -- e.g. dumping
// the failing compile command's stderr, or (with --jit-verbose) the
// generated source itself.
static void dumpfile(const string &label, const string &path) {
  ifstream f(path);
  if (!f)
    return;
  cout << "----- " << label << " (" << path << ") -----\n"
       << f.rdbuf() << "----- end " << label << " -----" << endl;
}
// On failure, prints (unless quiet) a one-line reason completing the
// "Compiling JIT . . . " line the caller already opened, then -- regardless
// of quiet, since these are genuine diagnostics rather than routine status
// -- dumps the compiler's own output so "JIT doesn't work here" is
// diagnosable instead of a silent fallback.
static bool compileandload(const string &src, void **handle, void ***conjtable,
                           void ***conjcmptable) {
  const char *tmpdir = getenv("TMPDIR");
  if (!tmpdir || !*tmpdir)
    tmpdir = "/tmp";
  string base = string(tmpdir) + "/twsearch_jit_XXXXXX";
  vector<char> templ(base.begin(), base.end());
  templ.push_back(0);
  int fd = mkstemp(templ.data());
  if (fd < 0)
    return false;
  string srcpath = templ.data();
  {
    ofstream f(srcpath);
    f << src;
  }
  close(fd);
  if (verbose > 1)
    dumpfile("generated JIT source", srcpath);
  string sopath = srcpath + ".so";
  bool ok = false;
  string failmsg = "compile failed; using interpreted.";
  // An explicit --jit-cc is a deliberate choice: try only that, and don't
  // paper over it failing by silently trying something else.  Otherwise,
  // fall through a short list of reasonable guesses.
  if (jitcc && *jitcc) {
    ok = trycompile(jitcc, srcpath, sopath);
  } else {
    const char *cxx = getenv("CXX");
    if (cxx && *cxx)
      ok = trycompile(cxx, srcpath, sopath);
    if (!ok)
      ok = trycompile("cc", srcpath, sopath);
    if (!ok)
      ok = trycompile("clang", srcpath, sopath);
    if (!ok)
      ok = trycompile("gcc", srcpath, sopath);
  }
  if (ok) {
    *handle = dlopen(sopath.c_str(), RTLD_NOW);
    ok = (*handle != 0);
    if (!ok) {
      const char *e = dlerror();
      failmsg = string("dlopen failed: ") + (e ? e : "unknown error") +
                "; using interpreted.";
    }
  }
  if (ok) {
    *conjtable = (void **)dlsym(*handle, "twsearch_jit_conjtable");
    *conjcmptable = (void **)dlsym(*handle, "twsearch_jit_conjcmptable");
    ok = (*conjtable != 0 && *conjcmptable != 0);
    if (!ok) {
      const char *e = dlerror();
      failmsg = string("dlsym failed: ") + (e ? e : "unknown error") +
                "; using interpreted.";
    }
  }
  if (!ok) {
    if (!quiet) // finishes the "Compiling JIT . . . " line the caller opened
      cout << failmsg << endl;
    // Preserve/print whatever the compiler had to say rather than just
    // discarding it -- otherwise "JIT doesn't work here" is undiagnosable.
    dumpfile("JIT compiler output", srcpath + ".log");
    if (verbose <= 1) // already shown above if verbose
      dumpfile("generated JIT source", srcpath);
  }
  unlink(srcpath.c_str());
  unlink((srcpath + ".log").c_str());
  unlink(sopath.c_str()); // safe post-dlopen; the mapping stays valid
  return ok;
}
// Check the generated functions against the interpreted originals on a
// batch of reachable random positions (and a few adversarial near-ties),
// for every rotation.  Anything less than a perfect match and we don't
// trust the JIT for this run.
//
// Uses its own local, fixed-seed RNG rather than myrand()/mysrand() so
// that attempting the JIT (whether it succeeds, fails, or isn't even
// applicable) never perturbs the shared random stream that -R makes
// reproducible for everything else (e.g. -T's timing tests, -S's random
// scrambles).
static bool selfcheck(puzdef &pd) {
  stacksetval p1(pd), tmp1(pd), tmp2(pd), d1(pd), d2(pd);
  pd.assignpos(p1, pd.solved);
  const int NPOS = 40;
  const int NROT = (int)pd.rotgroup.size();
  unsigned int localseed = 0x5bd1e995u;
  for (int t = 0; t < NPOS; t++) {
    if (t > 0) {
      localseed = localseed * 1103515245u + 12345u;
      int mv = (int)((localseed >> 8) % pd.moves.size());
      pd.mul(p1, pd.moves[mv].pos, tmp1);
      pd.assignpos(p1, tmp1);
    }
    for (int m = 0; m < NROT; m++) {
      pd.rotconjugate(pd.rotinvmap[m], p1, pd.rotgroup[m].pos, tmp1);
      pd.jitconj[m](p1.dat, tmp2.dat);
      if (pd.comparepos(tmp1, tmp2) != 0)
        return false;
      // conjcmp needs a "current best" to compare against; try a few,
      // including one guaranteed to tie (a copy of the true conjugate).
      for (int which = 0; which < 3; which++) {
        if (which == 0)
          pd.assignpos(d1, pd.solved);
        else if (which == 1)
          pd.assignpos(d1, tmp1); // forces the tie/no-write path
        else
          pd.assignpos(d1, p1);
        pd.assignpos(d2, d1);
        int r1 =
            pd.rotconjugatecmp(pd.rotinvmap[m], p1, pd.rotgroup[m].pos, d1);
        int r2 = pd.jitconjcmp[m](p1.dat, d2.dat);
        if (r1 != r2 || pd.comparepos(d1, d2) != 0)
          return false;
      }
    }
  }
  return true;
}
void jit_build_symmetry(puzdef &pd) {
  pd.jitconj.clear();
  pd.jitconjcmp.clear();
  if (!enablejit)
    return;
  int nrot = (int)pd.rotgroup.size();
  if (nrot < 1 || nrot > 64)
    return;
  for (auto &sd : pd.setdefs)
    if (sd.relabel)
      return; // not handled by the generated code (yet); stay interpreted
  string src = gensource(pd);
  void *handle = 0;
  void **conjtable = 0, **conjcmptable = 0;
  // Compiling can take several seconds for puzzles with large rotation
  // groups (e.g. megaminx), so say something before blocking on it -- and
  // keep the whole status (start, timing, outcome) to this one line, since
  // compileandload() completes it itself on failure.
  if (!quiet)
    cout << "Compiling JIT . . . " << flush;
  auto compilestart = chrono::steady_clock::now();
  bool compiled = compileandload(src, &handle, &conjtable, &conjcmptable);
  double secs =
      chrono::duration<double>(chrono::steady_clock::now() - compilestart)
          .count();
  if (!compiled)
    return; // compileandload() already finished the status line
  pd.jithandle = handle;
  pd.jitconj.resize(nrot);
  pd.jitconjcmp.resize(nrot);
  for (int m = 0; m < nrot; m++) {
    pd.jitconj[m] = (puzdef::jitconjfn_t)conjtable[m];
    pd.jitconjcmp[m] = (puzdef::jitconjcmpfn_t)conjcmptable[m];
  }
  if (!selfcheck(pd)) {
    if (!quiet)
      cout << "self-check failed; using interpreted." << endl;
    pd.jitconj.clear();
    pd.jitconjcmp.clear();
    return;
  }
  if (!quiet)
    cout << nrot << " rotations, " << fixed << setprecision(1) << secs
         << "s." << endl;
}
