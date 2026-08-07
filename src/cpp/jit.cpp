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
int jitstyle = 1;
const char *jitcc;
static int roundup(int x, int m) { return ((x + m - 1) / m) * m; }
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
// Table-indexed variant (see --jit-style 0): same fully-unrolled-per-element
// code shape as gensource() above -- still no loop over setdefs/j, so no
// loop-control overhead -- but *one* shared conj()/conjcmp() pair, selected
// at call time by a rotation index argument m, reading ap/cp out of flat
// [nrot][totsize] tables instead of having them baked in as per-rotation
// literals.  That reintroduces `cp[j]` as a genuine runtime load (`bp[cp[j]]`
// is back to two *dependent* loads instead of gensource()'s one
// fixed-offset load), so this is slower than gensource() -- but the
// generated code no longer scales with nrot, so it's a lot smaller: the
// data that used to make each rotation's code different now lives in
// AP[]/CP[], which are just read-only tables, not more code.
static string gensource_table(const puzdef &pd) {
  ostringstream o;
  o << "typedef unsigned char uc;\n";
  int nrot = (int)pd.rotgroup.size();
  int totsize = pd.totsize;
  o << "static const uc AP[" << nrot << "][" << totsize << "] = {\n";
  for (int m = 0; m < nrot; m++) {
    const uchar *ap = pd.rotinvmap[m].dat;
    o << "  {";
    for (int k = 0; k < totsize; k++)
      o << (k ? "," : "") << (int)ap[k];
    o << "},\n";
  }
  o << "};\n";
  o << "static const uc CP[" << nrot << "][" << totsize << "] = {\n";
  for (int m = 0; m < nrot; m++) {
    const uchar *cp = pd.rotgroup[m].pos.dat;
    o << "  {";
    for (int k = 0; k < totsize; k++)
      o << (k ? "," : "") << (int)cp[k];
    o << "},\n";
  }
  o << "};\n";
  set<int> omods;
  for (auto &sd : pd.setdefs)
    if (sd.omod > 1)
      omods.insert(sd.omod);
  for (int om : omods) {
    uchar *moda = gmoda[om];
    o << "static const uc MODA" << om << "[" << 4 * om << "] = {";
    for (int k = 0; k < 4 * om; k++)
      o << (k ? "," : "") << (int)moda[k];
    o << "};\n";
  }
  // Named tblconj/tblconjcmp rather than conj/conjcmp to avoid colliding
  // with the C library's own complex-math conj() builtin.
  o << "void tblconj(int m, const uc *bp, uc *dp) {\n";
  o << "  const uc *ap = AP[m], *cp = CP[m];\n";
  for (auto &sd : pd.setdefs) {
    int n = sd.size, off = sd.off;
    for (int j = 0; j < n; j++) {
      o << "  dp[" << (off + j) << "] = ap[" << off << "+bp[" << off
        << "+cp[" << (off + j) << "]]];\n";
      if (sd.omod > 1)
        o << "  dp[" << (off + n + j) << "] = MODA" << (int)sd.omod
          << "[ap[" << (off + n) << "+bp[" << off << "+cp[" << (off + j)
          << "]]]+MODA" << (int)sd.omod << "[bp[" << (off + n) << "+cp["
          << (off + j) << "]]+cp[" << (off + n + j) << "]]];\n";
      else
        o << "  dp[" << (off + n + j) << "] = 0;\n";
    }
  }
  o << "}\n";
  o << "int tblconjcmp(int m, const uc *bp, uc *dp) {\n";
  o << "  const uc *ap = AP[m], *cp = CP[m];\n";
  o << "  int r = 0;\n";
  for (auto &sd : pd.setdefs) {
    int n = sd.size, off = sd.off;
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
      if (sd.omod > 1) {
        o << "  { uc nv = MODA" << (int)sd.omod << "[ap[" << (off + n)
          << "+bp[" << off << "+cp[" << (off + j) << "]]]+MODA"
          << (int)sd.omod << "[bp[" << (off + n) << "+cp[" << (off + j)
          << "]]+cp[" << didx << "]]];\n";
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
// SIMD-gather variants (--jit-style 2/3): unlike gensource()/gensource_table
// above, these don't unroll the setdef/j loop at all -- the generated code
// is a small, fixed set of loops regardless of nrot or totsize, so code
// size stays flat (all the per-puzzle specificity lives in AP[]/GIDX[]/etc,
// which is data, not code).  The loops replace the interpreted/table
// versions' scalar `bp[cp[j]]` dependent-load chain with a vector gather:
// conceptually, TBL(source_table, index_vector) looks up 16 bytes at once
// out of up to a 64-byte (NEON) or 16-byte (SSE) table register; ORing the
// per-window results together reconstructs the correct byte, since exactly
// one window ever "owns" any given index (see gensource_neon/gensource_sse
// for how each ISA's semantics make that ORing safe).
//
// Both stages of `ap[bp[cp[j]]]` are gathers: first bp[] gathered by a
// per-position source-index table GIDX (== gensource()'s `gidx`, but global
// rather than folded into per-rotation literals), then AP[m][] gathered by
// (that result + a per-position setdef-offset table OFFV, since `off`
// varies by which setdef a given output byte belongs to).  Both bp and
// AP[m] can exceed one table register's width, hence the window loop.
//
// Orientation deltas involve two MODA[] table lookups and an add, not a
// pure gather, so they're left to an ordinary scalar patch pass afterward
// (loop-based over a small setdef descriptor table, not unrolled -- still
// code-size-flat) that overwrites every orientation byte the gather wrote
// something meaningless to, reusing the same GIDX/AP data.
//
// conjcmp reuses the exact same gather+patch to build a full candidate
// array, then finds the first differing byte against the current dp with a
// plain scalar walk -- equivalent to (but structured differently from)
// gensource()'s interleaved compare-and-lazily-write version.  Once SIMD is
// computing the whole candidate up front anyway, there's no remaining
// benefit to fusing the comparison into the gather itself.
//
// Emits everything except gather_core() itself (ISA-specific; the caller
// emits that separately) plus a forward declaration for it.
static void emit_simd_tables(ostringstream &o, const puzdef &pd,
                             int winbytes) {
  int nrot = (int)pd.rotgroup.size();
  int totsize = pd.totsize;
  int inbuf = roundup(totsize, winbytes);
  int nwin = inbuf / winbytes;
  int outbuf = roundup(totsize, 16);
  int nchunk = outbuf / 16;
  o << "#define TOTSIZE " << totsize << "\n";
  o << "#define INBUF " << inbuf << "\n";
  o << "#define OUTBUF " << outbuf << "\n";
  o << "#define NWIN " << nwin << "\n";
  o << "#define NCHUNK " << nchunk << "\n";
  o << "static const uc AP[" << nrot << "][INBUF] = {\n";
  for (int m = 0; m < nrot; m++) {
    const uchar *ap = pd.rotinvmap[m].dat;
    o << "  {";
    for (int k = 0; k < inbuf; k++)
      o << (k ? "," : "") << (k < totsize ? (int)ap[k] : 0);
    o << "},\n";
  }
  o << "};\n";
  // GIDX/OFFV are indexed by "off+j" -- a permutation slot -- with the
  // global source byte index (== gensource()'s `gidx`) / that setdef's
  // `off`, respectively.  ODELTA reuses the same "off+j" index space to
  // carry the matching orientation delta (gensource()'s `delta`).  Every
  // other position (each "off+n+j" orientation slot, and any padding past
  // totsize) is left 0 here; gather_core still computes *something* for
  // those lanes (there's no per-lane way to skip them in a vector store),
  // but the scalar patch pass below always overwrites them afterward.
  o << "static const uc GIDX[" << nrot << "][OUTBUF] = {\n";
  for (int m = 0; m < nrot; m++) {
    const uchar *cp = pd.rotgroup[m].pos.dat;
    vector<int> gidx(outbuf, 0);
    for (auto &sd : pd.setdefs) {
      int n = sd.size, off = sd.off;
      for (int j = 0; j < n; j++)
        gidx[off + j] = off + cp[off + j];
    }
    o << "  {";
    for (int k = 0; k < outbuf; k++)
      o << (k ? "," : "") << gidx[k];
    o << "},\n";
  }
  o << "};\n";
  {
    vector<int> offv(outbuf, 0);
    for (auto &sd : pd.setdefs) {
      int n = sd.size, off = sd.off;
      for (int j = 0; j < n; j++)
        offv[off + j] = off;
    }
    o << "static const uc OFFV[OUTBUF] = {";
    for (int k = 0; k < outbuf; k++)
      o << (k ? "," : "") << offv[k];
    o << "};\n";
  }
  set<int> omods;
  for (auto &sd : pd.setdefs)
    if (sd.omod > 1)
      omods.insert(sd.omod);
  for (int om : omods) {
    uchar *moda = gmoda[om];
    o << "static const uc MODA" << om << "[" << 4 * om << "] = {";
    for (int k = 0; k < 4 * om; k++)
      o << (k ? "," : "") << (int)moda[k];
    o << "};\n";
  }
  if (!omods.empty()) {
    o << "static const uc ODELTA[" << nrot << "][OUTBUF] = {\n";
    for (int m = 0; m < nrot; m++) {
      const uchar *cp = pd.rotgroup[m].pos.dat;
      vector<int> odelta(outbuf, 0);
      for (auto &sd : pd.setdefs) {
        if (sd.omod <= 1)
          continue;
        int n = sd.size, off = sd.off;
        for (int j = 0; j < n; j++)
          odelta[off + j] = cp[off + n + j];
      }
      o << "  {";
      for (int k = 0; k < outbuf; k++)
        o << (k ? "," : "") << odelta[k];
      o << "},\n";
    }
    o << "};\n";
  }
  o << "typedef struct { int off, n; const uc *moda; } orispec_t;\n";
  {
    int norispecs = 0;
    ostringstream os;
    for (auto &sd : pd.setdefs)
      if (sd.omod > 1) {
        if (norispecs)
          os << ",";
        os << "{" << sd.off << "," << sd.size << ",MODA" << (int)sd.omod
           << "}";
        norispecs++;
      }
    o << "#define NORISPECS " << norispecs << "\n";
    if (norispecs)
      o << "static const orispec_t ORISPECS[] = {" << os.str() << "};\n";
  }
  o << "typedef struct { int off, n; } zerospec_t;\n";
  {
    int nzerospecs = 0;
    ostringstream os;
    for (auto &sd : pd.setdefs)
      if (sd.omod <= 1) {
        if (nzerospecs)
          os << ",";
        os << "{" << sd.off << "," << sd.size << "}";
        nzerospecs++;
      }
    o << "#define NZEROSPECS " << nzerospecs << "\n";
    if (nzerospecs)
      o << "static const zerospec_t ZEROSPECS[] = {" << os.str() << "};\n";
  }
  o << "static void gather_core(const uc *ap, const uc *gidx, "
       "const uc *bpbuf, uc *outbuf);\n";
}
// ISA-agnostic: the orientation/zero patch pass, and the tblconj/tblconjcmp
// entry points (see the big comment above emit_simd_tables).  Emitted after
// gather_core() so it can call it.
static void emit_simd_wrappers(ostringstream &o) {
  o << "static void simd_patch(int m, const uc *bpbuf, uc *outbuf) {\n";
  o << "  (void)m; (void)bpbuf; (void)outbuf;\n";
  o << "#if NORISPECS\n";
  o << "  { const uc *ap = AP[m], *gidx = GIDX[m], *odelta = ODELTA[m];\n";
  o << "    int s, j;\n";
  o << "    for (s = 0; s < NORISPECS; s++) {\n";
  o << "      int off = ORISPECS[s].off, n = ORISPECS[s].n;\n";
  o << "      const uc *moda = ORISPECS[s].moda;\n";
  o << "      for (j = 0; j < n; j++) {\n";
  o << "        uc g = gidx[off+j];\n";
  o << "        outbuf[off+n+j] = moda[ap[off+n+bpbuf[g]] + "
       "moda[bpbuf[g+n]+odelta[off+j]]];\n";
  o << "      }\n";
  o << "    }\n";
  o << "  }\n";
  o << "#endif\n";
  o << "#if NZEROSPECS\n";
  o << "  { int s;\n";
  o << "    for (s = 0; s < NZEROSPECS; s++)\n";
  o << "      memset(outbuf + ZEROSPECS[s].off + ZEROSPECS[s].n, 0, "
       "ZEROSPECS[s].n);\n";
  o << "  }\n";
  o << "#endif\n";
  o << "}\n";
  o << "void tblconj(int m, const uc *bp, uc *dp) {\n";
  o << "  uc bpbuf[INBUF], outbuf[OUTBUF];\n";
  o << "  memset(bpbuf, 0, INBUF);\n";
  o << "  memcpy(bpbuf, bp, TOTSIZE);\n";
  o << "  gather_core(AP[m], GIDX[m], bpbuf, outbuf);\n";
  o << "  simd_patch(m, bpbuf, outbuf);\n";
  o << "  memcpy(dp, outbuf, TOTSIZE);\n";
  o << "}\n";
  o << "int tblconjcmp(int m, const uc *bp, uc *dp) {\n";
  o << "  uc bpbuf[INBUF], outbuf[OUTBUF];\n";
  o << "  int k;\n";
  o << "  memset(bpbuf, 0, INBUF);\n";
  o << "  memcpy(bpbuf, bp, TOTSIZE);\n";
  o << "  gather_core(AP[m], GIDX[m], bpbuf, outbuf);\n";
  o << "  simd_patch(m, bpbuf, outbuf);\n";
  o << "  for (k = 0; k < TOTSIZE; k++) {\n";
  o << "    if (outbuf[k] > dp[k]) return 1;\n";
  o << "    if (outbuf[k] < dp[k]) { memcpy(dp, outbuf, TOTSIZE); return "
       "-1; }\n";
  o << "  }\n";
  o << "  return 0;\n";
  o << "}\n";
}
// --jit-style 2: ARM NEON.  gather_core() windows bp/AP[m] into groups of
// up to 4 16-byte registers (vqtbl4q_u8's table operand, 64 bytes), and for
// each 16-byte output chunk, ORs together every window's vqtbl4q_u8 result.
// That's safe because vqtbl4q_u8 itself produces 0 for any index >= 64, and
// (idx - 64*w), computed as wraparound uint8 arithmetic, is >= 64 for every
// window that doesn't actually own idx (whether idx is beyond this window,
// or -- wrapping around -- before it): so at most one window ever
// contributes a nonzero byte to a given lane, and ORing them together
// recovers it (or correctly reconstructs a legitimate 0).
static string gensource_neon(const puzdef &pd) {
  ostringstream o;
  o << "#include <arm_neon.h>\n#include <string.h>\ntypedef unsigned char "
       "uc;\n";
  emit_simd_tables(o, pd, 64);
  o << "static void gather_core(const uc *ap, const uc *gidx, "
       "const uc *bpbuf, uc *outbuf) {\n";
  o << "  uint8x16x4_t bpwin[NWIN], apwin[NWIN];\n";
  o << "  int w, c;\n";
  o << "  for (w = 0; w < NWIN; w++) {\n";
  o << "    bpwin[w] = vld1q_u8_x4(bpbuf + 64*w);\n";
  o << "    apwin[w] = vld1q_u8_x4(ap + 64*w);\n";
  o << "  }\n";
  o << "  for (c = 0; c < NCHUNK; c++) {\n";
  o << "    uint8x16_t idx = vld1q_u8(gidx + 16*c);\n";
  o << "    uint8x16_t g1 = vdupq_n_u8(0);\n";
  o << "    for (w = 0; w < NWIN; w++) {\n";
  o << "      uint8x16_t local = vsubq_u8(idx, vdupq_n_u8((uc)(64*w)));\n";
  o << "      g1 = vorrq_u8(g1, vqtbl4q_u8(bpwin[w], local));\n";
  o << "    }\n";
  o << "    { uint8x16_t idx2 = vaddq_u8(g1, vld1q_u8(OFFV + 16*c));\n";
  o << "      uint8x16_t out = vdupq_n_u8(0);\n";
  o << "      for (w = 0; w < NWIN; w++) {\n";
  o << "        uint8x16_t local2 = vsubq_u8(idx2, "
       "vdupq_n_u8((uc)(64*w)));\n";
  o << "        out = vorrq_u8(out, vqtbl4q_u8(apwin[w], local2));\n";
  o << "      }\n";
  o << "      vst1q_u8(outbuf + 16*c, out);\n";
  o << "    }\n";
  o << "  }\n";
  o << "}\n";
  emit_simd_wrappers(o);
  return o.str();
}
// --jit-style 3: x86 SSSE3/SSE4.1.  Same idea as NEON's gather_core, but
// _mm_shuffle_epi8's table operand is only 16 bytes (one window == one
// output chunk, NWIN == NCHUNK), and -- unlike vqtbl4q_u8 -- it does *not*
// zero its result for every out-of-range index (only for index bytes with
// the high bit set; a raw index of, say, 20 would wrap into the table's
// low nibble instead).  So instead of relying on wraparound + the
// instruction's own zeroing, each window explicitly computes an in-range
// mask (unsigned local<16, via the standard "flip both operands' sign bit,
// then signed-compare" trick, since SSE has no unsigned byte compare) and
// blends with it, which is correct however it's out of range.
static string gensource_sse(const puzdef &pd) {
  ostringstream o;
  o << "#include <tmmintrin.h>\n#include <smmintrin.h>\n#include "
       "<string.h>\ntypedef unsigned char uc;\n";
  emit_simd_tables(o, pd, 16);
  o << "static void gather_core(const uc *ap, const uc *gidx, "
       "const uc *bpbuf, uc *outbuf) {\n";
  o << "  __m128i bpwin[NWIN], apwin[NWIN];\n";
  o << "  int w, c;\n";
  o << "  for (w = 0; w < NWIN; w++) {\n";
  o << "    bpwin[w] = _mm_loadu_si128((const __m128i *)(bpbuf + 16*w));\n";
  o << "    apwin[w] = _mm_loadu_si128((const __m128i *)(ap + 16*w));\n";
  o << "  }\n";
  o << "  for (c = 0; c < NCHUNK; c++) {\n";
  o << "    __m128i idx = _mm_loadu_si128((const __m128i *)(gidx + "
       "16*c));\n";
  o << "    __m128i g1 = _mm_setzero_si128();\n";
  o << "    for (w = 0; w < NWIN; w++) {\n";
  o << "      __m128i base = _mm_set1_epi8((char)(16*w));\n";
  o << "      __m128i local = _mm_sub_epi8(idx, base);\n";
  o << "      __m128i lb = _mm_xor_si128(local, _mm_set1_epi8((char)0x80));"
       "\n";
  o << "      __m128i inr = _mm_cmplt_epi8(lb, _mm_set1_epi8((char)0x90));"
       "\n";
  o << "      __m128i g = _mm_shuffle_epi8(bpwin[w], local);\n";
  o << "      g1 = _mm_blendv_epi8(g1, g, inr);\n";
  o << "    }\n";
  o << "    { __m128i offv = _mm_loadu_si128((const __m128i *)(OFFV + "
       "16*c));\n";
  o << "      __m128i idx2 = _mm_add_epi8(g1, offv);\n";
  o << "      __m128i out = _mm_setzero_si128();\n";
  o << "      for (w = 0; w < NWIN; w++) {\n";
  o << "        __m128i base = _mm_set1_epi8((char)(16*w));\n";
  o << "        __m128i local = _mm_sub_epi8(idx2, base);\n";
  o << "        __m128i lb = _mm_xor_si128(local, "
       "_mm_set1_epi8((char)0x80));\n";
  o << "        __m128i inr = _mm_cmplt_epi8(lb, "
       "_mm_set1_epi8((char)0x90));\n";
  o << "        __m128i g = _mm_shuffle_epi8(apwin[w], local);\n";
  o << "        out = _mm_blendv_epi8(out, g, inr);\n";
  o << "      }\n";
  o << "      _mm_storeu_si128((__m128i *)(outbuf + 16*c), out);\n";
  o << "    }\n";
  o << "  }\n";
  o << "}\n";
  emit_simd_wrappers(o);
  return o.str();
}
// Try each of these, in order, as a C compiler; the generated file is
// plain C, and passing -x c makes the language choice explicit regardless
// of the file's (extensionless) name, so a C++-flavored CXX still works.
// extraflags carries ISA-specific flags the generated code needs beyond
// the baseline (e.g. --jit-style 3's -mssse3 -msse4.1).
static bool trycompile(const string &cc, const string &srcpath,
                       const string &sopath, const string &extraflags) {
  string cmd = cc + " -x c -O2 -shared -fPIC " + extraflags + " -o " +
               sopath + " " + srcpath + " > " + srcpath + ".log 2>&1";
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
static bool compileandload(const string &src, const string &extraflags,
                           void **handle, const char *sym1, void **out1,
                           const char *sym2, void **out2) {
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
    ok = trycompile(jitcc, srcpath, sopath, extraflags);
  } else {
    const char *cxx = getenv("CXX");
    if (cxx && *cxx)
      ok = trycompile(cxx, srcpath, sopath, extraflags);
    if (!ok)
      ok = trycompile("cc", srcpath, sopath, extraflags);
    if (!ok)
      ok = trycompile("clang", srcpath, sopath, extraflags);
    if (!ok)
      ok = trycompile("gcc", srcpath, sopath, extraflags);
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
    *out1 = dlsym(*handle, sym1);
    *out2 = dlsym(*handle, sym2);
    ok = (*out1 != 0 && *out2 != 0);
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
      pd.jitconjcall(m, p1, tmp2);
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
        int r2 = pd.jitconjcmpcall(m, p1, d2);
        if (r1 != r2 || pd.comparepos(d1, d2) != 0)
          return false;
      }
    }
  }
  return true;
}
// Style 1 ("portable") uses jitconj/jitconjcmp (array of nrot function
// pointers, one call per rotation); every other style shares a single
// tblconj/tblconjcmp pair taking the rotation index as an argument
// (jitconjtable/jitconjcmptable).  See puzdef.h's havejit()/jitconjcall()/
// jitconjcmpcall(), which hide this distinction from every other caller.
static bool styleistableshaped(int style) { return style != 1; }
void jit_build_symmetry(puzdef &pd) {
  pd.jitconj.clear();
  pd.jitconjcmp.clear();
  pd.jitconjtable = 0;
  pd.jitconjcmptable = 0;
  if (!enablejit)
    return;
  int nrot = (int)pd.rotgroup.size();
  if (nrot < 1 || nrot > 64)
    return;
  for (auto &sd : pd.setdefs)
    if (sd.relabel)
      return; // not handled by the generated code (yet); stay interpreted
  if (jitstyle == 4) {
    if (!quiet)
      cout << "--jit-style 4 (avx512) isn't implemented yet; using "
              "interpreted."
           << endl;
    return;
  }
  string src;
  string extraflags;
  const char *stylename = "";
  switch (jitstyle) {
  case 0:
    src = gensource_table(pd);
    stylename = "table";
    break;
  case 2:
    src = gensource_neon(pd);
    stylename = "neon";
    break;
  case 3:
    src = gensource_sse(pd);
    extraflags = "-mssse3 -msse4.1";
    stylename = "sse";
    break;
  default:
    src = gensource(pd);
    break;
  }
  bool tableshaped = styleistableshaped(jitstyle);
  const char *sym1 = tableshaped ? "tblconj" : "twsearch_jit_conjtable";
  const char *sym2 = tableshaped ? "tblconjcmp" : "twsearch_jit_conjcmptable";
  void *handle = 0;
  void *out1 = 0, *out2 = 0;
  // Compiling can take several seconds for puzzles with large rotation
  // groups (e.g. megaminx), so say something before blocking on it -- and
  // keep the whole status (start, timing, outcome) to this one line, since
  // compileandload() completes it itself on failure.
  if (!quiet)
    cout << "Compiling JIT . . . " << flush;
  auto compilestart = chrono::steady_clock::now();
  bool compiled =
      compileandload(src, extraflags, &handle, sym1, &out1, sym2, &out2);
  double secs =
      chrono::duration<double>(chrono::steady_clock::now() - compilestart)
          .count();
  if (!compiled)
    return; // compileandload() already finished the status line
  pd.jithandle = handle;
  if (tableshaped) {
    pd.jitconjtable = (puzdef::jitconjtablefn_t)out1;
    pd.jitconjcmptable = (puzdef::jitconjcmptablefn_t)out2;
  } else {
    void **conjtable = (void **)out1, **conjcmptable = (void **)out2;
    pd.jitconj.resize(nrot);
    pd.jitconjcmp.resize(nrot);
    for (int m = 0; m < nrot; m++) {
      pd.jitconj[m] = (puzdef::jitconjfn_t)conjtable[m];
      pd.jitconjcmp[m] = (puzdef::jitconjcmpfn_t)conjcmptable[m];
    }
  }
  if (!selfcheck(pd)) {
    if (!quiet)
      cout << "self-check failed; using interpreted." << endl;
    pd.jitconj.clear();
    pd.jitconjcmp.clear();
    pd.jitconjtable = 0;
    pd.jitconjcmptable = 0;
    return;
  }
  if (!quiet)
    cout << nrot << " rotations" << (*stylename ? " (" : "") << stylename
         << (*stylename ? ")" : "") << ", " << fixed << setprecision(1)
         << secs << "s." << endl;
}
