// Standalone validation of the split-index permutation ranking scheme
// (see conversation / commit message): rank an n=8 permutation as
// LO[top4_raw] + HI[bottom4_raw], two O(1) table lookups instead of an
// O(n^2) (or O(n log n)) Lehmer-code computation.  Cross-checked against
// a naive reference rank/unrank over the full 8! domain.
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <vector>
using namespace std;
typedef uint8_t u8;
typedef array<u8, 8> Perm8;

static const int N = 8;
static const int FACT[9] = {1, 1, 2, 6, 24, 120, 720, 5040, 40320};

// Naive ground truth: standard Lehmer code, O(n^2).
static int rank_naive(const Perm8 &p) {
  int rank = 0;
  for (int i = 0; i < N; i++) {
    int c = 0;
    for (int j = i + 1; j < N; j++)
      if (p[j] < p[i])
        c++;
    rank += c * FACT[N - 1 - i];
  }
  return rank;
}
static Perm8 unrank_naive(int rank) {
  vector<int> avail(N);
  iota(avail.begin(), avail.end(), 0);
  Perm8 p;
  for (int i = 0; i < N; i++) {
    int f = FACT[N - 1 - i];
    int idx = rank / f;
    rank %= f;
    p[i] = avail[idx];
    avail.erase(avail.begin() + idx);
  }
  return p;
}

// Split-index tables.  Raw index = 4-digit base-8 number from the 4 raw
// byte values (only valid, i.e. all-distinct, entries are ever queried
// with real data, but we size for the full 8^4 range for O(1) direct
// indexing rather than a separate "compress to valid-only" step).
static const int HALFBASE = 8 * 8 * 8 * 8; // 4096
static int LO[HALFBASE], HI[HALFBASE];
static bool LOvalid[HALFBASE], HIvalid[HALFBASE];

static int raw4(u8 a, u8 b, u8 c, u8 d) { return ((a * 8 + b) * 8 + c) * 8 + d; }

static void build_tables() {
  fill(LOvalid, LOvalid + HALFBASE, false);
  fill(HIvalid, HIvalid + HALFBASE, false);
  // LO[top4]: 24 * (contribution of positions 0..3 to the full Lehmer
  // rank), which depends only on the top4 values themselves (see the
  // c_i = a_i - count(smaller among a_0..a_{i-1}) derivation).
  // Enumerate all ordered 4-tuples of distinct values from 0..7.
  int idxs[4];
  for (idxs[0] = 0; idxs[0] < 8; idxs[0]++)
    for (idxs[1] = 0; idxs[1] < 8; idxs[1]++) {
      if (idxs[1] == idxs[0])
        continue;
      for (idxs[2] = 0; idxs[2] < 8; idxs[2]++) {
        if (idxs[2] == idxs[0] || idxs[2] == idxs[1])
          continue;
        for (idxs[3] = 0; idxs[3] < 8; idxs[3]++) {
          if (idxs[3] == idxs[0] || idxs[3] == idxs[1] || idxs[3] == idxs[2])
            continue;
          int contrib = 0;
          for (int i = 0; i < 4; i++) {
            int c = 0;
            for (int k = 0; k < i; k++)
              if (idxs[k] < idxs[i])
                c++;
            int a_i_rank = idxs[i] - c; // c_i, per the derivation
            contrib += a_i_rank * FACT[N - 1 - i];
          }
          int r = raw4(idxs[0], idxs[1], idxs[2], idxs[3]);
          LO[r] = contrib;
          LOvalid[r] = true;
        }
      }
    }
  // HI[bottom4]: ordinary rank of the 4 raw values *relative to each
  // other* (map each to its rank among just these 4, then take the
  // standard 4-element Lehmer rank).  Depends only on relative order,
  // so works directly off the raw byte values regardless of which
  // specific 4 values they are.
  for (idxs[0] = 0; idxs[0] < 8; idxs[0]++)
    for (idxs[1] = 0; idxs[1] < 8; idxs[1]++) {
      if (idxs[1] == idxs[0])
        continue;
      for (idxs[2] = 0; idxs[2] < 8; idxs[2]++) {
        if (idxs[2] == idxs[0] || idxs[2] == idxs[1])
          continue;
        for (idxs[3] = 0; idxs[3] < 8; idxs[3]++) {
          if (idxs[3] == idxs[0] || idxs[3] == idxs[1] || idxs[3] == idxs[2])
            continue;
          int rel[4];
          for (int i = 0; i < 4; i++) {
            int c = 0;
            for (int k = 0; k < 4; k++)
              if (k != i && idxs[k] < idxs[i])
                c++;
            rel[i] = c; // relative rank 0..3 of idxs[i] among the 4
          }
          int contrib = 0;
          for (int i = 0; i < 4; i++) {
            int c = 0;
            for (int k = i + 1; k < 4; k++) // AFTER i, per the actual
              if (rel[k] < rel[i])          // Lehmer code definition --
                c++;                        // LO used a "subtract from
            contrib += c * FACT[3 - i];     // total" shortcut that
          }                                 // doesn't apply here.
          int r = raw4(idxs[0], idxs[1], idxs[2], idxs[3]);
          HI[r] = contrib;
          HIvalid[r] = true;
        }
      }
    }
}

static int rank_split(const Perm8 &p) {
  int t = raw4(p[0], p[1], p[2], p[3]);
  int b = raw4(p[4], p[5], p[6], p[7]);
  return LO[t] + HI[b];
}

int main() {
  build_tables();
  // 1. Every valid top4/bottom4 raw pattern got a table entry.
  int lo_valid_count = 0, hi_valid_count = 0;
  for (int i = 0; i < HALFBASE; i++) {
    lo_valid_count += LOvalid[i];
    hi_valid_count += HIvalid[i];
  }
  printf("LO valid entries: %d (expect 1680)\n", lo_valid_count);
  printf("HI valid entries: %d (expect 1680)\n", hi_valid_count);
  assert(lo_valid_count == 1680);
  assert(hi_valid_count == 1680);

  // 2. Exhaustively check every one of the 40320 permutations: the split
  // rank must match the naive rank exactly, AND unranking the naive rank
  // must round-trip.
  vector<int> seen(40320, -1);
  int mismatches = 0;
  for (int r = 0; r < 40320; r++) {
    Perm8 p = unrank_naive(r);
    int rn = rank_naive(p);
    assert(rn == r); // sanity on the reference itself
    int rs = rank_split(p);
    if (rs != r) {
      if (mismatches < 5)
        printf("MISMATCH: naive rank %d, split rank %d\n", r, rs);
      mismatches++;
    }
    if (rs >= 0 && rs < 40320) {
      if (seen[rs] != -1) {
        printf("COLLISION: split rank %d produced by both naive-rank %d and %d\n",
               rs, seen[rs], r);
      }
      seen[rs] = r;
    } else {
      printf("OUT OF RANGE: naive rank %d -> split rank %d\n", r, rs);
    }
  }
  printf("mismatches: %d / 40320\n", mismatches);
  int uncovered = 0;
  for (int r = 0; r < 40320; r++)
    if (seen[r] == -1)
      uncovered++;
  printf("uncovered split ranks: %d / 40320\n", uncovered);
  printf("%s\n", (mismatches == 0 && uncovered == 0) ? "ALL PASS" : "FAILURES FOUND");
  return (mismatches || uncovered) ? 1 : 0;
}
