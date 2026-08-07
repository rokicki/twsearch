// Standalone (no twsearch dependency) prototype/benchmark for candidate
// "which rotation(s) tie for lexicographically least at output position 0
// of the discriminator set" algorithms -- i.e. the mandatory first round
// of lowsymmbits, on real per-puzzle rotation data extracted by
// extract.cpp.  Build/run: `make bench` (see Makefile).
//
// Every algorithm implements build(const PuzzleData&) [setup, not timed]
// and query(const uint8_t *obs) -> uint64_t [the thing we're timing].
// Correctness is checked against extract.cpp's precomputed reference
// (a direct, deliberately-unclever O(nrot) scan) on every sample before
// any algorithm's timing is trusted.
//
// IMPORTANT correctness subtlety, worth stating explicitly: an algorithm
// that exits as soon as it finds the true minimum *value* (e.g. the
// position-indexed table scan below, breaking out the moment it sees an
// achievable value of 0) is NOT guaranteed to return the *complete* tie
// bitmask -- other, later-checked sources might also tie at that same
// value, and we'd never know because we stopped looking. That's a real
// correctness gap (same orbit could resolve to different representatives
// depending on where the scan happened to find its first zero), not just
// an approximation. We measure and report it honestly rather than
// silently trusting early-exit variants: exact-match algorithms must
// match the reference bitmask on every sample; early-exit variants are
// checked for "same winning value, and returned mask is a non-empty
// subset of the true mask" and we report how often that's the only thing
// true (i.e. how often it under-reports the tie set).
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>
#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
using namespace std;
using u8 = uint8_t;
using u64 = uint64_t;

struct PuzzleData {
  string name;
  int n = 0, nrot = 0;
  int discoff = 0; // discriminator set's offset within the full totsize
                    // state -- G[]/AP[] use indices local to its own
                    // n-byte slice, so a full-state array needs slicing
                    // at this offset before being handed to a guess algo.
  vector<array<u8, 64>> G, AP; // [m][j], j<n valid, rest 0
  vector<array<u8, 64>> obs;   // samples, [i][j], j<n valid, rest 0
  vector<u64> ref;             // reference bitmask per sample
};

static void loadsamples(PuzzleData &pd, const string &path) {
  ifstream f(path, ios::binary);
  if (!f) {
    cerr << "! can't open " << path << endl;
    exit(1);
  }
  int rec = pd.n + 8;
  f.seekg(0, ios::end);
  long bytes = f.tellg();
  f.seekg(0, ios::beg);
  long count = bytes / rec;
  pd.obs.assign(count, {});
  pd.ref.assign(count, 0);
  vector<char> buf(rec);
  for (long i = 0; i < count; i++) {
    f.read(buf.data(), rec);
    memcpy(pd.obs[i].data(), buf.data(), pd.n);
    memcpy(&pd.ref[i], buf.data() + pd.n, 8);
  }
}

#include "cube333_edges_rotdata.h"
#include "cube333_rotdata.h"
#include "fto_rotdata.h"
#include "megaminx_corners_rotdata.h"
#include "megaminx_edges_rotdata.h"

template <int N, int NROT>
static void fillrot(PuzzleData &pd, int discoff, const u8 (&Gsrc)[NROT][N],
                     const u8 (&APsrc)[NROT][N]) {
  pd.n = N;
  pd.nrot = NROT;
  pd.discoff = discoff;
  pd.G.assign(NROT, {});
  pd.AP.assign(NROT, {});
  for (int m = 0; m < NROT; m++) {
    for (int j = 0; j < N; j++) {
      pd.G[m][j] = Gsrc[m][j];
      pd.AP[m][j] = APsrc[m][j];
    }
  }
}

// ---------------------------------------------------------------------
// Full-pipeline data (all setdefs, not just the discriminator set):
// everything needed to reproduce rotconjugate/rotconjugatecmp, so we can
// measure guess + actual conjugation together, not just the guess.
// ---------------------------------------------------------------------
struct FullData {
  string name;
  int totsize = 0, nrot = 0, nsetdefs = 0;
  vector<int> sdsize, sdoff, sdomod;
  vector<vector<u8>> FULLCP, FULLAP; // [m][totsize]
  vector<vector<u8>> fullobs;        // full-state samples, [i][totsize]
  vector<u64> guessref;              // position-0 guess reference, per sample
  vector<vector<u8>> conjref;        // true slowmodm2() result, [i][totsize]
};

// gmoda[omod] is a pure function of omod (see readksolve.cpp): the first
// 2*omod entries are i%omod, the rest are the "don't care" sentinel
// 2*omod.  No need to load it from the extractor; just recompute it.
static vector<u8> makemoda(int omod) {
  vector<u8> t(4 * omod);
  for (int i = 0; i < 2 * omod; i++)
    t[i] = i % omod;
  for (int i = 2 * omod; i < 4 * omod; i++)
    t[i] = 2 * omod;
  return t;
}

template <int TOTSIZE, int NROT, int NSD>
static void fillfull(FullData &fd, const int (&sdsize)[NSD], const int (&sdoff)[NSD],
                     const int (&sdomod)[NSD], const u8 (&CP)[NROT][TOTSIZE],
                     const u8 (&AP)[NROT][TOTSIZE]) {
  fd.totsize = TOTSIZE;
  fd.nrot = NROT;
  fd.nsetdefs = NSD;
  fd.sdsize.assign(sdsize, sdsize + NSD);
  fd.sdoff.assign(sdoff, sdoff + NSD);
  fd.sdomod.assign(sdomod, sdomod + NSD);
  fd.FULLCP.assign(NROT, vector<u8>(TOTSIZE));
  fd.FULLAP.assign(NROT, vector<u8>(TOTSIZE));
  for (int m = 0; m < NROT; m++)
    for (int j = 0; j < TOTSIZE; j++) {
      fd.FULLCP[m][j] = CP[m][j];
      fd.FULLAP[m][j] = AP[m][j];
    }
}

static void loadfullsamples(FullData &fd, const string &path) {
  ifstream f(path, ios::binary);
  if (!f) {
    cerr << "! can't open " << path << endl;
    exit(1);
  }
  int rec = fd.totsize + 8 + fd.totsize;
  f.seekg(0, ios::end);
  long bytes = f.tellg();
  f.seekg(0, ios::beg);
  long count = bytes / rec;
  fd.fullobs.assign(count, vector<u8>(fd.totsize));
  fd.guessref.assign(count, 0);
  fd.conjref.assign(count, vector<u8>(fd.totsize));
  vector<char> buf(rec);
  for (long i = 0; i < count; i++) {
    f.read(buf.data(), rec);
    memcpy(fd.fullobs[i].data(), buf.data(), fd.totsize);
    memcpy(&fd.guessref[i], buf.data() + fd.totsize, 8);
    memcpy(fd.conjref[i].data(), buf.data() + fd.totsize + 8, fd.totsize);
  }
}

// ---------------------------------------------------------------------
// Candidate algorithms
// ---------------------------------------------------------------------

// Today's original interpreted lowsymmbits mandatory round, before
// yesterday's branch-free rewrite: if/else-if, real branches.
struct AlgoBranchy {
  const PuzzleData *pd;
  static constexpr const char *name = "branchy (original)";
  static constexpr bool exact = true;
  void build(const PuzzleData &p) { pd = &p; }
  u64 query(const u8 *obs) const {
    int rv = pd->AP[0][obs[pd->G[0][0]]];
    u64 r = 1;
    for (int m = 1; m < pd->nrot; m++) {
      int t = pd->AP[m][obs[pd->G[m][0]]];
      if (t < rv) {
        r = 1ULL << m;
        rv = t;
      } else if (t == rv)
        r |= 1ULL << m;
    }
    return r;
  }
};

// Currently committed on symm-combined: same single pass, ternaries
// instead of if/else-if (compiles to csel, no data-dependent branches).
struct AlgoTernary {
  const PuzzleData *pd;
  static constexpr const char *name = "ternary (committed)";
  static constexpr bool exact = true;
  void build(const PuzzleData &p) { pd = &p; }
  u64 query(const u8 *obs) const {
    int rv = pd->AP[0][obs[pd->G[0][0]]];
    u64 r = 1;
    for (int m = 1; m < pd->nrot; m++) {
      int t = pd->AP[m][obs[pd->G[m][0]]];
      u64 bit = 1ULL << m;
      bool isless = t < rv, iseq = t == rv;
      rv = isless ? t : rv;
      r = isless ? bit : (iseq ? (r | bit) : r);
    }
    return r;
  }
};

// BestAtSource[s][v] = min over {m : G[m][0]==s} of AP[m][v], plus which
// rotation(s) achieve it.  O(n^2) table (no n! blowup), O(1) build per
// (s,v) pair updated once per rotation (O(n*nrot) total).  Query is O(n):
// exact global minimum via min-over-partition, always checking every
// source position -- correct and complete, no early-exit gap.
struct AlgoBestAtSource {
  int n = 0;
  vector<u8> val;   // n*n
  vector<u64> mask; // n*n
  static constexpr const char *name = "BestAtSource (full scan)";
  static constexpr bool exact = true;
  void build(const PuzzleData &p) {
    n = p.n;
    val.assign((size_t)n * n, 255);
    mask.assign((size_t)n * n, 0);
    for (int m = 0; m < p.nrot; m++) {
      int s = p.G[m][0];
      for (int v = 0; v < n; v++) {
        int idx = s * n + v;
        int t = p.AP[m][v];
        if (t < val[idx]) {
          val[idx] = t;
          mask[idx] = 1ULL << m;
        } else if (t == val[idx])
          mask[idx] |= 1ULL << m;
      }
    }
  }
  u64 query(const u8 *obs) const {
    int rv = 256;
    u64 r = 0;
    for (int s = 0; s < n; s++) {
      int idx = s * n + obs[s];
      int t = val[idx];
      if (t < rv) {
        rv = t;
        r = mask[idx];
      } else if (t == rv)
        r |= mask[idx];
    }
    return r;
  }
};

// Same table, but stop at the first source position achieving the
// (unbeatable, once seen) value 0.  Deliberately NOT exact -- see the
// file header comment.  Included to *measure* how often that matters,
// not because we trust it as-is.
struct AlgoBestAtSourceEarlyExit {
  int n = 0;
  vector<u8> val;
  vector<u64> mask;
  static constexpr const char *name = "BestAtSource (early exit @0)";
  static constexpr bool exact = false;
  void build(const PuzzleData &p) {
    n = p.n;
    val.assign((size_t)n * n, 255);
    mask.assign((size_t)n * n, 0);
    for (int m = 0; m < p.nrot; m++) {
      int s = p.G[m][0];
      for (int v = 0; v < n; v++) {
        int idx = s * n + v;
        int t = p.AP[m][v];
        if (t < val[idx]) {
          val[idx] = t;
          mask[idx] = 1ULL << m;
        } else if (t == val[idx])
          mask[idx] |= 1ULL << m;
      }
    }
  }
  u64 query(const u8 *obs) const {
    int rv = 256;
    u64 r = 0;
    for (int s = 0; s < n; s++) {
      int idx = s * n + obs[s];
      int t = val[idx];
      if (t < rv) {
        rv = t;
        r = mask[idx];
        if (rv == 0)
          break;
      } else if (t == rv)
        r |= mask[idx];
    }
    return r;
  }
};

#ifdef __ARM_NEON
// Rotation-indexed: for target t = 0, 1, 2, ..., gather obs[G[m][0]] for
// all nrot<=64 rotations in one vqtbl4q_u8 (reused across every target
// level -- it doesn't depend on t), compare against a precomputed
// "what value would rotation m need to see to score exactly t" vector,
// and take the first t with any match.  Each target-level check tests
// all (up to 64) lanes at once via the vector compare, so unlike the
// position-indexed early-exit above, there's no partial-check gap: the
// mask returned for the first successful t is always complete.
struct AlgoNeonTarget {
  int n = 0, nrot = 0;
  vector<u8> src;              // 64, padded with 255 (out-of-range -> 0)
  vector<array<u8, 64>> want;  // n x 64, padded with 255 (never matches)
  static constexpr const char *name = "NEON target-gather";
  static constexpr bool exact = true;
  void build(const PuzzleData &p) {
    n = p.n;
    nrot = p.nrot;
    src.assign(64, 255);
    for (int m = 0; m < nrot; m++)
      src[m] = p.G[m][0];
    want.assign(n, array<u8, 64>{});
    for (auto &w : want)
      w.fill(255);
    for (int t = 0; t < n; t++) {
      for (int m = 0; m < nrot; m++) {
        int v = -1;
        for (int vv = 0; vv < n; vv++)
          if (p.AP[m][vv] == t) {
            v = vv;
            break;
          }
        want[t][m] = (u8)v; // AP[m] is a bijection on [0,n): always found
      }
    }
  }
  u64 query(const u8 *obs) const {
    u8 buf[64] = {0};
    memcpy(buf, obs, n);
    uint8x16x4_t table = {vld1q_u8(buf), vld1q_u8(buf + 16), vld1q_u8(buf + 32),
                          vld1q_u8(buf + 48)};
    uint8x16_t g0 = vqtbl4q_u8(table, vld1q_u8(src.data()));
    uint8x16_t g1 = vqtbl4q_u8(table, vld1q_u8(src.data() + 16));
    uint8x16_t g2 = vqtbl4q_u8(table, vld1q_u8(src.data() + 32));
    uint8x16_t g3 = vqtbl4q_u8(table, vld1q_u8(src.data() + 48));
    for (int t = 0; t < n; t++) {
      const u8 *w = want[t].data();
      uint8x16_t c0 = vceqq_u8(g0, vld1q_u8(w));
      uint8x16_t c1 = vceqq_u8(g1, vld1q_u8(w + 16));
      uint8x16_t c2 = vceqq_u8(g2, vld1q_u8(w + 32));
      uint8x16_t c3 = vceqq_u8(g3, vld1q_u8(w + 48));
      uint8x16_t any = vorrq_u8(vorrq_u8(c0, c1), vorrq_u8(c2, c3));
      if (vmaxvq_u8(any) == 0)
        continue;
      u64 mask = 0;
      u8 tmp[16];
      vst1q_u8(tmp, c0);
      for (int i = 0; i < 16; i++)
        if (tmp[i])
          mask |= 1ULL << i;
      vst1q_u8(tmp, c1);
      for (int i = 0; i < 16; i++)
        if (tmp[i])
          mask |= 1ULL << (16 + i);
      vst1q_u8(tmp, c2);
      for (int i = 0; i < 16; i++)
        if (tmp[i])
          mask |= 1ULL << (32 + i);
      vst1q_u8(tmp, c3);
      for (int i = 0; i < 16; i++)
        if (tmp[i])
          mask |= 1ULL << (48 + i);
      if (nrot < 64)
        mask &= (1ULL << nrot) - 1;
      return mask;
    }
    return 0; // unreachable: AP is a bijection, some t always matches
  }
};
#endif

// ---------------------------------------------------------------------
// Conjugate implementations: given the winning rotation's index, actually
// produce the symmetry-reduced state (rotconjugate), and -- for the rare
// case the guess leaves more than one rotation tied -- compare-and-
// conditionally-write against the current best (rotconjugatecmp).  Both
// mirror puzdef.h's committed implementations exactly, just reading from
// FullData instead of a live puzdef.
// ---------------------------------------------------------------------
struct InterpretedConjugate {
  const FullData *fd;
  vector<vector<u8>> moda; // moda[omod], only for omod>1 values in play
  static constexpr const char *name = "interpreted (flat tables)";
  void build(const FullData &f) {
    fd = &f;
    int maxomod = 0;
    for (int om : fd->sdomod)
      maxomod = max(maxomod, om);
    moda.assign(maxomod + 1, {});
    for (int om : fd->sdomod)
      if (om > 1 && moda[om].empty())
        moda[om] = makemoda(om);
  }
  void conjugate(int m, const u8 *bp, u8 *dp) const {
    const u8 *ap = fd->FULLAP[m].data();
    const u8 *cp = fd->FULLCP[m].data();
    for (int si = 0; si < fd->nsetdefs; si++) {
      int n = fd->sdsize[si], off = fd->sdoff[si], om = fd->sdomod[si];
      for (int j = 0; j < n; j++) {
        int gidx = off + cp[off + j];
        dp[off + j] = ap[off + bp[gidx]];
        if (om > 1) {
          int delta = cp[off + n + j];
          dp[off + n + j] =
              moda[om][ap[off + n + bp[gidx]] + moda[om][bp[gidx + n] + delta]];
        } else {
          dp[off + n + j] = 0;
        }
      }
    }
  }
  // Returns -1/0/1 like rotconjugatecmp; may partially overwrite dp even
  // when returning 1 (matches committed semantics: callers only trust dp
  // when the return is <= 0).
  int conjugatecmp(int m, const u8 *bp, u8 *dp) const {
    const u8 *ap = fd->FULLAP[m].data();
    const u8 *cp = fd->FULLCP[m].data();
    int r = 0;
    for (int si = 0; si < fd->nsetdefs; si++) {
      int n = fd->sdsize[si], off = fd->sdoff[si], om = fd->sdomod[si];
      for (int j = 0; j < n; j++) {
        int gidx = off + cp[off + j];
        int didx = off + j;
        int nv = ap[off + bp[gidx]];
        if (r > 0)
          dp[didx] = nv;
        else if (nv > dp[didx])
          return 1;
        else if (nv < dp[didx]) {
          r = 1;
          dp[didx] = nv;
        }
      }
      for (int j = 0; j < n; j++) {
        int didx = off + n + j;
        int nv;
        if (om > 1) {
          int gidx = off + cp[off + j];
          int delta = cp[off + n + j];
          nv = moda[om][ap[off + n + bp[gidx]] + moda[om][bp[gidx + n] + delta]];
        } else {
          nv = 0;
        }
        if (r > 0)
          dp[didx] = nv;
        else if (nv > dp[didx])
          return 1;
        else if (nv < dp[didx]) {
          r = 1;
          dp[didx] = nv;
        }
      }
    }
    return -r;
  }
};

// .so paths kept around (not unlinked) so their on-disk __TEXT size can be
// inspected after the benchmark run (e.g. `otool -l <path> | grep -A4
// 'sectname __text'` on macOS) -- printed at the end of main().
static vector<pair<string, string>> keptSoPaths; // (label, path)

// JIT: generate C source baking every rotation's cp/ap tables in as
// compile-time constants (identical codegen to src/cpp/jit.cpp's
// gensource(), just reading from FullData instead of a live puzdef),
// compile with the system C compiler, dlopen it back in.
struct JitConjugate {
  const FullData *fd;
  void *handle = nullptr;
  vector<void (*)(const u8 *, u8 *)> conjfn;
  vector<int (*)(const u8 *, u8 *)> conjcmpfn;
  static constexpr const char *name = "JIT (compiled per rotation)";
  static string gensource(const FullData &fd) {
    ostringstream o;
    o << "typedef unsigned char uc;\n";
    for (int m = 0; m < fd.nrot; m++) {
      o << "static const uc AP" << m << "[" << fd.totsize << "] = {";
      for (int k = 0; k < fd.totsize; k++)
        o << (k ? "," : "") << (int)fd.FULLAP[m][k];
      o << "};\n";
    }
    vector<int> omods;
    for (int om : fd.sdomod)
      if (om > 1 && find(omods.begin(), omods.end(), om) == omods.end())
        omods.push_back(om);
    for (int om : omods) {
      auto moda = makemoda(om);
      o << "static const uc MODA" << om << "[" << 4 * om << "] = {";
      for (int k = 0; k < 4 * om; k++)
        o << (k ? "," : "") << (int)moda[k];
      o << "};\n";
    }
    for (int m = 0; m < fd.nrot; m++) {
      const u8 *cp = fd.FULLCP[m].data();
      o << "void conj" << m << "(const uc *bp, uc *dp) {\n";
      for (int si = 0; si < fd.nsetdefs; si++) {
        int n = fd.sdsize[si], off = fd.sdoff[si], om = fd.sdomod[si];
        for (int j = 0; j < n; j++) {
          int gidx = off + cp[off + j];
          int didx = off + j;
          o << "  dp[" << didx << "] = AP" << m << "[" << off << "+bp[" << gidx
            << "]];\n";
          if (om > 1) {
            int delta = cp[off + n + j];
            o << "  dp[" << (didx + n) << "] = MODA" << om << "[AP" << m << "["
              << (off + n) << "+bp[" << gidx << "]]+MODA" << om << "[bp["
              << (gidx + n) << "]+" << delta << "]];\n";
          } else {
            o << "  dp[" << (didx + n) << "] = 0;\n";
          }
        }
      }
      o << "}\n";
      o << "int conjcmp" << m << "(const uc *bp, uc *dp) {\n";
      o << "  int r = 0;\n";
      for (int si = 0; si < fd.nsetdefs; si++) {
        int n = fd.sdsize[si], off = fd.sdoff[si], om = fd.sdomod[si];
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
          if (om > 1) {
            int gidx = off + cp[off + j];
            int delta = cp[off + n + j];
            o << "  { uc nv = MODA" << om << "[AP" << m << "[" << (off + n)
              << "+bp[" << gidx << "]]+MODA" << om << "[bp[" << (gidx + n) << "]+"
              << delta << "]];\n";
            o << "    if (r > 0) dp[" << didx << "] = nv;\n";
            o << "    else if (nv > dp[" << didx << "]) return 1;\n";
            o << "    else if (nv < dp[" << didx << "]) { r = 1; dp[" << didx
              << "] = nv; } }\n";
          } else {
            o << "  if (r > 0) dp[" << didx << "] = 0;\n";
            o << "  else if (dp[" << didx << "] != 0) { r = 1; dp[" << didx
              << "] = 0; }\n";
          }
        }
      }
      o << "  return -r;\n}\n";
    }
    o << "void (*bench_jit_conjtable[])(const uc*, uc*) = {";
    for (int m = 0; m < fd.nrot; m++)
      o << (m ? "," : "") << "conj" << m;
    o << "};\n";
    o << "int (*bench_jit_conjcmptable[])(const uc*, uc*) = {";
    for (int m = 0; m < fd.nrot; m++)
      o << (m ? "," : "") << "conjcmp" << m;
    o << "};\n";
    return o.str();
  }
  static bool trycompile(const string &cc, const string &srcpath,
                         const string &sopath) {
    string cmd = cc + " -x c -O2 -shared -fPIC -o " + sopath + " " + srcpath +
                 " > " + srcpath + ".log 2>&1";
    return system(cmd.c_str()) == 0;
  }
  void build(const FullData &f) {
    fd = &f;
    string src = gensource(f);
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir || !*tmpdir)
      tmpdir = "/tmp";
    string base = string(tmpdir) + "/minrot_jit_XXXXXX";
    vector<char> templ(base.begin(), base.end());
    templ.push_back(0);
    int fdesc = mkstemp(templ.data());
    if (fdesc < 0) {
      cerr << "! mkstemp failed" << endl;
      exit(1);
    }
    string srcpath = templ.data();
    {
      ofstream o(srcpath);
      o << src;
    }
    close(fdesc);
    string sopath = srcpath + ".so";
    bool ok = trycompile("cc", srcpath, sopath);
    if (!ok)
      ok = trycompile("clang", srcpath, sopath);
    if (!ok) {
      cerr << "! JIT compile failed, see " << srcpath << ".log" << endl;
      exit(1);
    }
    handle = dlopen(sopath.c_str(), RTLD_NOW);
    unlink(srcpath.c_str());
    unlink((srcpath + ".log").c_str());
    keptSoPaths.push_back({name, sopath}); // deliberately not unlinked
    if (!handle) {
      cerr << "! dlopen failed: " << dlerror() << endl;
      exit(1);
    }
    auto conjtable = (void (**)(const u8 *, u8 *))dlsym(handle, "bench_jit_conjtable");
    auto conjcmptable =
        (int (**)(const u8 *, u8 *))dlsym(handle, "bench_jit_conjcmptable");
    if (!conjtable || !conjcmptable) {
      cerr << "! dlsym failed" << endl;
      exit(1);
    }
    conjfn.assign(conjtable, conjtable + f.nrot);
    conjcmpfn.assign(conjcmptable, conjcmptable + f.nrot);
  }
  void conjugate(int m, const u8 *bp, u8 *dp) const { conjfn[m](bp, dp); }
  int conjugatecmp(int m, const u8 *bp, u8 *dp) const { return conjcmpfn[m](bp, dp); }
};

// Third option: still fully unroll over every element of every setdef (no
// loop over j, so no loop-control/branch overhead, same as JitConjugate
// above) -- but *one* conj()/conjcmp() pair, shared across all rotations,
// reading cp/ap from flat [nrot][totsize] tables indexed by an m argument
// instead of having cp baked in as a literal (which is what forces
// JitConjugate to duplicate the code nrot times).  Trades speed for size:
// `bp[cp[j]]` becomes `bp[off+CP[m][off+j]]` -- CP[m][off+j] is now a
// genuine runtime load, so this is back to the same 3-dependent-loads
// pattern InterpretedConjugate has, just without that version's setdef/j
// loop overhead.  Code size should be ~1/nrot of JitConjugate's.
struct JitTableConjugate {
  const FullData *fd;
  void *handle = nullptr;
  void (*conjfn)(int, const u8 *, u8 *) = nullptr;
  int (*conjcmpfn)(int, const u8 *, u8 *) = nullptr;
  static constexpr const char *name = "JIT (shared routine, table-indexed)";
  static string gensource(const FullData &fd) {
    ostringstream o;
    o << "typedef unsigned char uc;\n";
    o << "static const uc AP[" << fd.nrot << "][" << fd.totsize << "] = {\n";
    for (int m = 0; m < fd.nrot; m++) {
      o << "  {";
      for (int k = 0; k < fd.totsize; k++)
        o << (k ? "," : "") << (int)fd.FULLAP[m][k];
      o << "},\n";
    }
    o << "};\n";
    o << "static const uc CP[" << fd.nrot << "][" << fd.totsize << "] = {\n";
    for (int m = 0; m < fd.nrot; m++) {
      o << "  {";
      for (int k = 0; k < fd.totsize; k++)
        o << (k ? "," : "") << (int)fd.FULLCP[m][k];
      o << "},\n";
    }
    o << "};\n";
    vector<int> omods;
    for (int om : fd.sdomod)
      if (om > 1 && find(omods.begin(), omods.end(), om) == omods.end())
        omods.push_back(om);
    for (int om : omods) {
      auto moda = makemoda(om);
      o << "static const uc MODA" << om << "[" << 4 * om << "] = {";
      for (int k = 0; k < 4 * om; k++)
        o << (k ? "," : "") << (int)moda[k];
      o << "};\n";
    }
    o << "void tblconj(int m, const uc *bp, uc *dp) {\n";
    o << "  const uc *ap = AP[m], *cp = CP[m];\n";
    for (int si = 0; si < fd.nsetdefs; si++) {
      int n = fd.sdsize[si], off = fd.sdoff[si], om = fd.sdomod[si];
      for (int j = 0; j < n; j++) {
        o << "  dp[" << (off + j) << "] = ap[" << off << "+bp[" << off
          << "+cp[" << (off + j) << "]]];\n";
        if (om > 1)
          o << "  dp[" << (off + n + j) << "] = MODA" << om << "[ap[" << (off + n)
            << "+bp[" << off << "+cp[" << (off + j) << "]]]+MODA" << om << "[bp["
            << (off + n) << "+cp[" << (off + j) << "]]+cp[" << (off + n + j)
            << "]]];\n";
        else
          o << "  dp[" << (off + n + j) << "] = 0;\n";
      }
    }
    o << "}\n";
    o << "int tblconjcmp(int m, const uc *bp, uc *dp) {\n";
    o << "  const uc *ap = AP[m], *cp = CP[m];\n";
    o << "  int r = 0;\n";
    for (int si = 0; si < fd.nsetdefs; si++) {
      int n = fd.sdsize[si], off = fd.sdoff[si], om = fd.sdomod[si];
      for (int j = 0; j < n; j++) {
        int didx = off + j;
        o << "  { uc nv = ap[" << off << "+bp[" << off << "+cp[" << didx
          << "]]];\n";
        o << "    if (r > 0) dp[" << didx << "] = nv;\n";
        o << "    else if (nv > dp[" << didx << "]) return 1;\n";
        o << "    else if (nv < dp[" << didx << "]) { r = 1; dp[" << didx
          << "] = nv; } }\n";
      }
      for (int j = 0; j < n; j++) {
        int didx = off + n + j;
        if (om > 1) {
          o << "  { uc nv = MODA" << om << "[ap[" << (off + n) << "+bp[" << off
            << "+cp[" << (off + j) << "]]]+MODA" << om << "[bp[" << (off + n)
            << "+cp[" << (off + j) << "]]+cp[" << didx << "]]];\n";
          o << "    if (r > 0) dp[" << didx << "] = nv;\n";
          o << "    else if (nv > dp[" << didx << "]) return 1;\n";
          o << "    else if (nv < dp[" << didx << "]) { r = 1; dp[" << didx
            << "] = nv; } }\n";
        } else {
          o << "  if (r > 0) dp[" << didx << "] = 0;\n";
          o << "  else if (dp[" << didx << "] != 0) { r = 1; dp[" << didx
            << "] = 0; }\n";
        }
      }
    }
    o << "  return -r;\n}\n";
    return o.str();
  }
  static bool trycompile(const string &cc, const string &srcpath,
                         const string &sopath) {
    string cmd = cc + " -x c -O2 -shared -fPIC -o " + sopath + " " + srcpath +
                 " > " + srcpath + ".log 2>&1";
    return system(cmd.c_str()) == 0;
  }
  void build(const FullData &f) {
    fd = &f;
    string src = gensource(f);
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir || !*tmpdir)
      tmpdir = "/tmp";
    string base = string(tmpdir) + "/minrot_jittab_XXXXXX";
    vector<char> templ(base.begin(), base.end());
    templ.push_back(0);
    int fdesc = mkstemp(templ.data());
    if (fdesc < 0) {
      cerr << "! mkstemp failed" << endl;
      exit(1);
    }
    string srcpath = templ.data();
    {
      ofstream o(srcpath);
      o << src;
    }
    close(fdesc);
    string sopath = srcpath + ".so";
    bool ok = trycompile("cc", srcpath, sopath);
    if (!ok)
      ok = trycompile("clang", srcpath, sopath);
    if (!ok) {
      cerr << "! JIT compile failed, see " << srcpath << ".log" << endl;
      exit(1);
    }
    handle = dlopen(sopath.c_str(), RTLD_NOW);
    unlink(srcpath.c_str());
    unlink((srcpath + ".log").c_str());
    keptSoPaths.push_back({name, sopath}); // deliberately not unlinked
    if (!handle) {
      cerr << "! dlopen failed: " << dlerror() << endl;
      exit(1);
    }
    conjfn = (void (*)(int, const u8 *, u8 *))dlsym(handle, "tblconj");
    conjcmpfn = (int (*)(int, const u8 *, u8 *))dlsym(handle, "tblconjcmp");
    if (!conjfn || !conjcmpfn) {
      cerr << "! dlsym failed" << endl;
      exit(1);
    }
  }
  void conjugate(int m, const u8 *bp, u8 *dp) const { conjfn(m, bp, dp); }
  int conjugatecmp(int m, const u8 *bp, u8 *dp) const { return conjcmpfn(m, bp, dp); }
};

// Combine a guess algorithm (position-0 only, as benchmarked above) with a
// conjugate implementation, mirroring slowmodm2's own control flow: guess
// picks a candidate bitmask; conjugate the first candidate unconditionally;
// conjugatecmp each further tied candidate.  Simplification vs. the
// committed code: we don't extend the guess to further discriminator-set
// elements on a tie (lowsymmbits' second loop) -- any tie surviving
// position 0 goes straight to full-state conjugatecmp here.  That's
// pessimistic on the (rare) tied case, not the common one, so it shouldn't
// distort the headline "is the guess step worth optimizing" comparison.
template <typename GuessAlgo, typename ConjugateImpl> struct FullPipeline {
  GuessAlgo guess;
  ConjugateImpl conj;
  int discoff = 0;
  static string name() {
    return string(GuessAlgo::name) + " + " + ConjugateImpl::name;
  }
  void build(const PuzzleData &pd, const FullData &fd) {
    guess.build(pd);
    conj.build(fd);
    discoff = pd.discoff;
  }
  // obs is the *full* totsize-byte observed state; writes the totsize-byte
  // symmetry-reduced result into dp.  The guess algorithms index relative
  // to the discriminator set's own slice, hence the +discoff.
  void query(const u8 *obs, u8 *dp) const {
    u64 bits = guess.query(obs + discoff);
    int g = __builtin_ffsll(bits) - 1;
    bits &= ~(1ULL << g);
    conj.conjugate(g, obs, dp);
    while (bits) {
      int m = __builtin_ffsll(bits) - 1;
      bits &= ~(1ULL << m);
      conj.conjugatecmp(m, obs, dp);
    }
  }
};

// ---------------------------------------------------------------------
// Harness
// ---------------------------------------------------------------------
template <typename Algo> static void run(PuzzleData &pd) {
  Algo algo;
  auto t0 = chrono::steady_clock::now();
  algo.build(pd);
  auto t1 = chrono::steady_clock::now();
  double buildms = chrono::duration<double, milli>(t1 - t0).count();

  // Correctness pass (not timed).
  size_t exactmatch = 0, mismatches = 0, subsetok = 0, valuewrong = 0;
  for (size_t i = 0; i < pd.obs.size(); i++) {
    u64 got = algo.query(pd.obs[i].data());
    u64 want = pd.ref[i];
    if (got == want) {
      exactmatch++;
      continue;
    }
    if (Algo::exact) {
      mismatches++;
      continue;
    }
    // early-exit variants: value must match, mask must be a non-empty
    // subset of the true tie set.
    if (got != 0 && (got & ~want) == 0)
      subsetok++;
    else
      valuewrong++;
  }

  // Timing pass: repeat over the sample set enough times for a stable
  // number.  Every rep does *identical* work (same pd.obs), so without a
  // barrier the optimizer can (and, verified, does) prove the outer loop
  // is redundant and either hoist the whole computation out or fold
  // "accumulate the same value `reps` times" into a multiply -- either
  // way silently invalidating the measurement (we saw exactly this:
  // several algorithms reporting 0.00ns/"inf M/s" on the first run).
  // doNotOptimize forces the compiler to treat `sink` as observable after
  // every single query, so it can't prove any call is redundant or hoist
  // it out of the rep loop.
  const int reps = 20;
  u64 sink = 0;
  t0 = chrono::steady_clock::now();
  for (int r = 0; r < reps; r++)
    for (auto &o : pd.obs) {
      sink += algo.query(o.data());
      asm volatile("" : : "r"(sink) : "memory");
    }
  t1 = chrono::steady_clock::now();
  double secs = chrono::duration<double>(t1 - t0).count();
  double nspq = secs * 1e9 / (reps * pd.obs.size());
  double mqps = 1000.0 / nspq;

  printf("  %-28s build %7.2fms  %6.2f ns/query  %6.2f M/s", Algo::name,
         buildms, nspq, mqps);
  if (Algo::exact) {
    if (mismatches)
      printf("   MISMATCH x%zu !!\n", mismatches);
    else
      printf("   (exact, verified on %zu samples)\n", pd.obs.size());
  } else {
    printf("   (exact %zu, incomplete-but-valid-subset %zu, WRONG x%zu / %zu)\n",
           exactmatch, subsetok, valuewrong, pd.obs.size());
  }
}

template <typename Pipeline>
static void runfull(const PuzzleData &pd, const FullData &fd) {
  Pipeline pipe;
  auto t0 = chrono::steady_clock::now();
  pipe.build(pd, fd);
  auto t1 = chrono::steady_clock::now();
  double buildms = chrono::duration<double, milli>(t1 - t0).count();

  size_t mismatches = 0;
  vector<u8> dp(fd.totsize);
  for (size_t i = 0; i < fd.fullobs.size(); i++) {
    pipe.query(fd.fullobs[i].data(), dp.data());
    if (memcmp(dp.data(), fd.conjref[i].data(), fd.totsize) != 0)
      mismatches++;
  }

  const int reps = 20;
  u64 sink = 0;
  t0 = chrono::steady_clock::now();
  for (int r = 0; r < reps; r++)
    for (auto &o : fd.fullobs) {
      pipe.query(o.data(), dp.data());
      sink += dp[0];
      asm volatile("" : : "r"(sink) : "memory");
    }
  t1 = chrono::steady_clock::now();
  double secs = chrono::duration<double>(t1 - t0).count();
  double nspq = secs * 1e9 / (reps * fd.fullobs.size());
  printf("  %-42s build %7.2fms  %7.2f ns/query  %6.2f M/s", Pipeline::name().c_str(),
         buildms, nspq, 1000.0 / nspq);
  if (mismatches)
    printf("   MISMATCH x%zu / %zu !!\n", mismatches, fd.fullobs.size());
  else
    printf("   (exact, verified on %zu samples)\n", fd.fullobs.size());
}

static void bench1full(const char *label, PuzzleData &pd, FullData &fd) {
  cout << "=== " << label << " full pipeline (guess + conjugate; totsize="
       << fd.totsize << ") ===" << endl;
  runfull<FullPipeline<AlgoBranchy, JitConjugate>>(pd, fd);
  runfull<FullPipeline<AlgoTernary, JitConjugate>>(pd, fd);
  runfull<FullPipeline<AlgoBestAtSource, JitConjugate>>(pd, fd);
  runfull<FullPipeline<AlgoBestAtSourceEarlyExit, JitConjugate>>(pd, fd);
#ifdef __ARM_NEON
  runfull<FullPipeline<AlgoNeonTarget, JitConjugate>>(pd, fd);
#endif
  runfull<FullPipeline<AlgoTernary, JitTableConjugate>>(pd, fd);
#ifdef __ARM_NEON
  runfull<FullPipeline<AlgoNeonTarget, JitTableConjugate>>(pd, fd);
#endif
  runfull<FullPipeline<AlgoTernary, InterpretedConjugate>>(pd, fd);
#ifdef __ARM_NEON
  runfull<FullPipeline<AlgoNeonTarget, InterpretedConjugate>>(pd, fd);
#endif
  cout << endl;
}

static void bench1(const char *label, PuzzleData &pd) {
  cout << "=== " << label << " (n=" << pd.n << " nrot=" << pd.nrot
       << " samples=" << pd.obs.size() << ") ===" << endl;
  run<AlgoBranchy>(pd);
  run<AlgoTernary>(pd);
  run<AlgoBestAtSource>(pd);
  run<AlgoBestAtSourceEarlyExit>(pd);
#ifdef __ARM_NEON
  run<AlgoNeonTarget>(pd);
#endif
  cout << endl;
}

int main() {
  PuzzleData cube333, fto, mmcorners, mmedges;
  fillrot<cube333_n, cube333_nrot>(cube333, cube333_disc_off, cube333_G,
                                   cube333_AP);
  loadsamples(cube333, "cube333_samples.bin");
  fillrot<fto_n, fto_nrot>(fto, fto_disc_off, fto_G, fto_AP);
  loadsamples(fto, "fto_samples.bin");
  fillrot<megaminx_corners_n, megaminx_corners_nrot>(
      mmcorners, megaminx_corners_disc_off, megaminx_corners_G,
      megaminx_corners_AP);
  loadsamples(mmcorners, "megaminx_corners_samples.bin");
  fillrot<megaminx_edges_n, megaminx_edges_nrot>(
      mmedges, megaminx_edges_disc_off, megaminx_edges_G, megaminx_edges_AP);
  loadsamples(mmedges, "megaminx_edges_samples.bin");

  bench1("3x3x3 corners", cube333);
  bench1("FTO C4RNER", fto);
  bench1("megaminx corners", mmcorners);
  bench1("megaminx edges", mmedges);

  // Full-pipeline (guess + real conjugation) benchmark: only valid for a
  // discriminator that *is* setdefs[0] (see the big comment on
  // fullsamples.bin generation in extract.cpp) -- EDGES for 3x3x3 and
  // megaminx, C4RNER for FTO.  cube333_edges is a separate extraction
  // from cube333 (which stays CORNERS-discriminated, valid for the
  // guess-only benchmark above) specifically for this.  "megaminx
  // corners" has no fullsamples.bin (extract.cpp skips generating one
  // for non-setdefs[0] discriminators) and is intentionally not
  // full-pipeline-benchmarked here.
  PuzzleData cube333e;
  fillrot<cube333_edges_n, cube333_edges_nrot>(cube333e, cube333_edges_disc_off,
                                               cube333_edges_G, cube333_edges_AP);

  FullData fcube333e, ffto, fmmedges;
  fcube333e.name = "cube333_edges";
  fillfull<cube333_edges_totsize, cube333_edges_nrot, cube333_edges_nsetdefs>(
      fcube333e, cube333_edges_sd_size, cube333_edges_sd_off,
      cube333_edges_sd_omod, cube333_edges_FULLCP, cube333_edges_FULLAP);
  loadfullsamples(fcube333e, "cube333_edges_fullsamples.bin");
  ffto.name = "fto";
  fillfull<fto_totsize, fto_nrot, fto_nsetdefs>(ffto, fto_sd_size, fto_sd_off,
                                                fto_sd_omod, fto_FULLCP,
                                                fto_FULLAP);
  loadfullsamples(ffto, "fto_fullsamples.bin");
  fmmedges.name = "megaminx_edges";
  fillfull<megaminx_edges_totsize, megaminx_edges_nrot, megaminx_edges_nsetdefs>(
      fmmedges, megaminx_edges_sd_size, megaminx_edges_sd_off,
      megaminx_edges_sd_omod, megaminx_edges_FULLCP, megaminx_edges_FULLAP);
  loadfullsamples(fmmedges, "megaminx_edges_fullsamples.bin");

  bench1full("3x3x3 (EDGES guess)", cube333e, fcube333e);
  bench1full("FTO (C4RNER guess)", fto, ffto);
  bench1full("megaminx (EDGES guess)", mmedges, fmmedges);

  cout << "=== compiled .so sizes (kept on disk for inspection) ===" << endl;
  for (auto &kv : keptSoPaths)
    cout << "  " << kv.first << ": " << kv.second << endl;
  return 0;
}
