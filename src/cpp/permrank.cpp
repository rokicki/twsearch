#include "permrank.h"
#include "util.h"
#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
#include <numeric>
#include <unordered_map>
int enablefastindex;
long long fastindexmaxn;
// Splits n into a "lo" half of size n/2 and a "hi" half of size n-n/2;
// lo/hi are direct-indexed (raw bytes read as a base-n number) rather
// than compressed to only the n!/(n-k)! valid (all-distinct) entries,
// trading a little wasted space for O(1) indexing with no extra
// translation step.
struct PermRank {
  int n = 0, split = 0;
  vector<long long> lo, hi;
  vector<long long> fact;
  void build(int n_) {
    n = n_;
    split = n / 2;
    int hisize = n - split;
    fact.assign(n + 1, 1);
    for (int i = 1; i <= n; i++)
      fact[i] = fact[i - 1] * i;
    long long losz = 1;
    for (int i = 0; i < split; i++)
      losz *= n;
    long long hisz = 1;
    for (int i = 0; i < hisize; i++)
      hisz *= n;
    lo.assign(losz, 0);
    hi.assign(hisz, 0);
    // lo[top]: the first `split` positions' contribution to the full
    // Lehmer-code rank.  Position i's true coefficient c_i (count of
    // smaller elements anywhere later in the *whole* n-length
    // permutation) equals idxs[i] itself (exactly that many values in
    // the whole 0..n-1 alphabet are smaller) minus however many of
    // those smaller values already appeared earlier in this same half
    // (idxs[0..i-1]) -- since every value not yet placed is guaranteed
    // to appear somewhere later, whether in the rest of this half or in
    // the second half, so nothing about the second half's arrangement
    // matters here.
    {
      vector<int> idxs(split);
      vector<bool> used(n, false);
      function<void(int)> rec = [&](int depth) {
        if (depth == split) {
          long long contrib = 0;
          for (int i = 0; i < split; i++) {
            int c = 0;
            for (int k = 0; k < i; k++)
              if (idxs[k] < idxs[i])
                c++;
            contrib += (long long)(idxs[i] - c) * fact[n - 1 - i];
          }
          long long raw = 0;
          for (int i = 0; i < split; i++)
            raw = raw * n + idxs[i];
          lo[raw] = contrib;
          return;
        }
        for (int v = 0; v < n; v++) {
          if (used[v])
            continue;
          used[v] = true;
          idxs[depth] = v;
          rec(depth + 1);
          used[v] = false;
        }
      };
      if (split > 0)
        rec(0);
      else
        lo[0] = 0; // degenerate empty lo half
    }
    // hi[bottom]: ordinary rank of the second half's raw values
    // *relative to each other* -- map each to its rank (0..hisize-1)
    // among just these hisize values, then take the standard Lehmer
    // rank of that.  Depends only on relative order, so it's a pure
    // function of the raw bytes regardless of which specific values
    // they are (unlike lo[], this genuinely doesn't need the "subtract
    // from total" trick since there's nothing after the second half to
    // account for).
    {
      vector<int> idxs(hisize);
      vector<bool> used(n, false);
      function<void(int)> rec = [&](int depth) {
        if (depth == hisize) {
          vector<int> rel(hisize);
          for (int i = 0; i < hisize; i++) {
            int c = 0;
            for (int k = 0; k < hisize; k++)
              if (k != i && idxs[k] < idxs[i])
                c++;
            rel[i] = c;
          }
          long long contrib = 0;
          for (int i = 0; i < hisize; i++) {
            int c = 0;
            for (int k = i + 1; k < hisize; k++) // AFTER i -- see the
              if (rel[k] < rel[i])               // permrank_proto.cpp
                c++;                             // history for why this
            contrib += c * fact[hisize - 1 - i]; // direction matters.
          }
          long long raw = 0;
          for (int i = 0; i < hisize; i++)
            raw = raw * n + idxs[i];
          hi[raw] = contrib;
          return;
        }
        for (int v = 0; v < n; v++) {
          if (used[v])
            continue;
          used[v] = true;
          idxs[depth] = v;
          rec(depth + 1);
          used[v] = false;
        }
      };
      if (hisize > 0)
        rec(0);
      else
        hi[0] = 0; // degenerate empty hi half
    }
  }
  long long size() const { return fact.empty() ? 0 : fact[n]; }
  int rank(const uchar *perm) const {
    long long t = 0;
    for (int i = 0; i < split; i++)
      t = t * n + perm[i];
    long long b = 0;
    int hisize = n - split;
    for (int i = 0; i < hisize; i++)
      b = b * n + perm[split + i];
    return (int)(lo[t] + hi[b]);
  }
};
static PermRank pr; // single active instance; twsearch loads one puzzle
                    // per process, so no need for this to live on puzdef.
int fastrank(const uchar *perm) { return pr.rank(perm); }
void build_fastindex(puzdef &pd) {
  pd.fastbits.clear();
  pd.fastbitsside.clear();
  if (!enablefastindex)
    return;
  if (pd.setdefs.empty() || pd.rotgroup.empty() || pd.moves.empty())
    return;
  int n = pd.setdefs[0].size;
  if (n < 2) // nothing to index
    return;
  // Check n! *before* building anything (LO/HI split tables included --
  // n^(n/2), while normally much smaller than n!, is still a real
  // allocation) -- a puzzle whose setdefs[0] just happens to be large
  // (e.g. megaminx's 30-element EDGES) must never reach an allocation
  // call here.  n>20 is a hard ceiling independent of --fastindex-maxn:
  // 21! already overflows a 64-bit count, so there's no cap value that
  // would make it safe to even compute, let alone allocate.
  if (n > 20) {
    if (!quiet)
      cout << "Fast symmetry index: " << pd.setdefs[0].name << " has " << n
           << " elements; " << n
           << "! isn't representable in 64 bits, skipping." << endl;
    return;
  }
  long long total = 1;
  for (int i = 2; i <= n; i++)
    total *= i; // safe: n<=20 guaranteed above, 20! fits comfortably
  if (fastindexmaxn > 0 && total > fastindexmaxn) {
    if (!quiet)
      cout << "Fast symmetry index: " << pd.setdefs[0].name << " has " << n
           << "! = " << total << " permutations, over --fastindex-maxn "
           << fastindexmaxn << "; skipping." << endl;
    return;
  }
  pr.build(n);
  int nrot = (int)pd.rotgroup.size();
  // Enumerate every permutation of setdefs[0] in lexicographic order via
  // std::next_permutation, which conveniently visits them in exactly
  // rank order -- so the loop counter *is* the rank, no separate
  // ranking or unranking needed to build the table itself (only at
  // query time, via fastrank() above).  Raw ull results go in a
  // temporary array first; fastbits/fastbitsside get built from that
  // once every distinct tie value has been seen (see below).
  vector<ull> rawbits((size_t)total);
  {
    stacksetval synth(pd);
    pd.assignpos(synth, pd.solved);
    vector<uchar> perm(n);
    iota(perm.begin(), perm.end(), (uchar)0);
    long long r = 0;
    do {
      memcpy(synth.dat, perm.data(), n);
      rawbits[(size_t)r] = pd.lowsymmbits(synth);
      r++;
    } while (next_permutation(perm.begin(), perm.end()));
    // Sanity check on the build loop itself (independent of
    // lowsymmbits' correctness): every rank got visited exactly once,
    // in order.
    if (r != total) {
      if (!quiet)
        cout << "Fast symmetry index: build loop visited " << r << " of "
             << total << " permutations; not using it." << endl;
      return;
    }
  }
  if (verbose > 1) {
    // Popcount histogram: how discriminating is setdefs[0] alone?  A
    // large fraction of multi-bit (tied) entries means slowmodm2 falls
    // through to the full-state rotconjugatecmp scan often even with
    // this table, which is the case this table can't speed up.
    long long hist[256] = {0};
    for (long long r = 0; r < total; r++)
      hist[__builtin_popcountll(rawbits[(size_t)r])]++;
    cout << "Fast symmetry index popcount histogram (" << pd.setdefs[0].name
         << ", " << total << " permutations):" << endl;
    for (int i = 0; i < 256; i++)
      if (hist[i])
        cout << "  " << i << " bit" << (i == 1 ? "" : "s") << ": " << hist[i]
             << " (" << (100.0 * hist[i] / total) << "%)" << endl;
  }
  // fastbitsside[0..nrot) is exactly 1LL<<i; every single-bit rawbits[]
  // value already has a slot there.  Ties (more than one bit set) get
  // deduplicated and appended after -- almost always a small handful of
  // distinct values in practice, per the same reasoning that makes
  // lowsymmbits' own extend-on-tie loop rare.  A byte can only index
  // 256 slots total; if that's ever not enough this is a hard error
  // (see below) rather than a silent fallback, since it'd mean the
  // "ties are rare and few" premise this whole encoding rests on
  // turned out to be wrong for some puzzle -- worth knowing loudly.
  pd.fastbitsside.assign(nrot, 0);
  for (int i = 0; i < nrot; i++)
    pd.fastbitsside[i] = 1ULL << i;
  unordered_map<ull, int> tieindex; // raw multi-bit value -> side-table slot
  pd.fastbits.resize((size_t)total);
  for (long long r = 0; r < total; r++) {
    ull bits = rawbits[(size_t)r];
    if ((bits & (bits - 1)) == 0) { // exactly one bit set (bits != 0 always)
      pd.fastbits[(size_t)r] = (uchar)(ffsll((long long)bits) - 1);
      continue;
    }
    auto it = tieindex.find(bits);
    int slot;
    if (it != tieindex.end()) {
      slot = it->second;
    } else {
      slot = nrot + (int)tieindex.size();
      if (slot > 255)
        error("! fast symmetry index: more distinct tie values than a "
              "byte can index -- unexpected, see permrank.cpp");
      tieindex[bits] = slot;
      pd.fastbitsside.push_back(bits);
    }
    pd.fastbits[(size_t)r] = (uchar)slot;
  }
  // Verify against the interpreted lowsymmbits() on real reachable
  // positions before trusting this -- same safety-net shape as jit.cpp's
  // selfcheck(), including using a local fixed-seed RNG rather than
  // myrand()/mysrand() so this never perturbs the shared random stream
  // -R makes reproducible for everything else.
  stacksetval p1(pd), tmp1(pd);
  pd.assignpos(p1, pd.solved);
  unsigned int localseed = 0xdeadbeefu;
  bool ok = true;
  for (int t = 0; t < 500 && ok; t++) {
    if (t > 0) {
      localseed = localseed * 1103515245u + 12345u;
      int mv = (int)((localseed >> 8) % pd.moves.size());
      pd.mul(p1, pd.moves[mv].pos, tmp1);
      pd.assignpos(p1, tmp1);
    }
    ull got = pd.fastbitsside[pd.fastbits[(size_t)fastrank(p1.dat)]];
    if (pd.lowsymmbits(p1) != got)
      ok = false;
  }
  if (!ok) {
    if (!quiet)
      cout << "Fast symmetry index self-check failed; using lowsymmbits()."
           << endl;
    pd.fastbits.clear();
    pd.fastbitsside.clear();
    return;
  }
  if (!quiet)
    cout << "Fast symmetry index built for " << pd.setdefs[0].name << " ("
         << total << " entries, " << pd.fastbitsside.size()
         << "-entry side table)." << endl;
}
