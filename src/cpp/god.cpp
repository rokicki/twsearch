#include "god.h"
#include "antipode.h"
#include "canon.h"
#include "cmds.h"
#include "index.h"
#include "readksolve.h"
#include "rotations.h"
#include "threads.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <map>
#include <unordered_map>
/*
 *   God's algorithm using two bits per state.
 */
vector<ull> cnts, scnts;
static vector<allocsetval> posns;
static vector<int> movehist;
void dotwobitgod(puzdef &pd) {
  ull nlongs = (pd.llstates + 31) >> 5;
  ull memneeded = nlongs * 8;
  ull *mem = (ull *)malloc(memneeded);
  if (mem == 0)
    error("! not enough memory");
  memset(mem, -1, memneeded);
  stacksetval p1(pd), p2(pd);
  pd.assignpos(p1, pd.solved);
  ull off = densepack(pd, p1);
  mem[off >> 5] -= 3LL << (2 * (off & 31));
  cnts.clear();
  cnts.push_back(1);
  ull tot = 1;
  for (int d = 0;; d++) {
    resetantipodes();
    cout << "Dist " << d << " cnt " << cnts[d] << " tot " << tot << " in "
         << duration() << endl
         << flush;
    if (cnts[d] == 0 || (pd.logstates <= 62 && tot == pd.llstates))
      break;
    ull newseen = 0;
    // don't be too aggressive, because we might see parity and this might slow
    // things down dramatically; only go backwards after more than 50% full.
    int back = (pd.logstates <= 62 && tot * 2 > pd.llstates);
    int seek = d % 3;
    int newv = (d + 1) % 3;
    if (back) {
      for (ull bigi = 0; bigi < nlongs; bigi++) {
        ull checkv = mem[bigi];
        checkv = (checkv & 0x5555555555555555LL) &
                 ((checkv >> 1) & 0x5555555555555555LL);
        for (int smi = ffsll(checkv); checkv; smi = ffsll(checkv)) {
          checkv -= 1LL << (smi - 1);
          denseunpack(pd, (bigi << 5) + (smi >> 1), p1);
          for (int i = 0; i < (int)pd.moves.size(); i++) {
            if (quarter && pd.moves[i].cost > 1)
              continue;
            pd.mul(p1, pd.moves[i].pos, p2);
            off = densepack(pd, p2);
            int v = 3 & (mem[off >> 5] >> (2 * (off & 31)));
            if (v == seek) {
              newseen++;
              stashantipodedense((bigi << 5) + (smi >> 1));
              mem[bigi] -= (3LL - newv) << (smi - 1);
              break;
            }
          }
        }
      }
    } else {
      ull xorv = (3 - seek) * 0x5555555555555555LL;
      for (ull bigi = 0; bigi < nlongs; bigi++) {
        if (mem[bigi] == 0xffffffffffffffffLL)
          continue;
        ull checkv = mem[bigi] ^ xorv;
        checkv = (checkv & 0x5555555555555555LL) &
                 ((checkv >> 1) & 0x5555555555555555LL);
        for (int smi = ffsll(checkv); checkv; smi = ffsll(checkv)) {
          checkv -= 1LL << (smi - 1);
          denseunpack(pd, (bigi << 5) + (smi >> 1), p1);
          for (int i = 0; i < (int)pd.moves.size(); i++) {
            if (quarter && pd.moves[i].cost > 1)
              continue;
            pd.mul(p1, pd.moves[i].pos, p2);
            off = densepack(pd, p2);
            int v = 3 & (mem[off >> 5] >> (2 * (off & 31)));
            if (v == 3) {
              newseen++;
              stashantipodedense(off);
              mem[off >> 5] -= (3LL - newv) << (2 * (off & 31));
            }
          }
        }
      }
    }
    cnts.push_back(newseen);
    tot += newseen;
  }
  showantipodesdense(pd, 0);
}
/*
 *   God's algorithm using two bits per state, but we also try to decompose
 *   the state so we can use symcoords at the lowest level, for speed.
 */
ull symcoordgoal = 20000;
int numsym = 0;
ll symcoordsize = 0;
int nmoves;
vector<int> movemap;
ull newseen;
unsigned int *symc;
ull *mem;
void innerloop(int back, int seek, int newv, ull sofar, vector<ull> &muld) {
  sofar *= symcoordsize;
  for (int i = 0; i < nmoves; i++)
    muld[i] *= symcoordsize;
  unsigned int *symtab = symc;
  if (back) {
    for (int smoff = 0; smoff < symcoordsize; smoff++, symtab += nmoves) {
      ull off = sofar + smoff;
      int v = 3 & (mem[off >> 5] >> (2 * (off & 31)));
      if (v == 3) {
        for (int m = 0; m < nmoves; m++) {
          ull off2 = muld[m] + symtab[m];
          int v2 = 3 & (mem[off2 >> 5] >> (2 * (off2 & 31)));
          if (v2 == seek) {
            mem[off >> 5] -= (3LL - newv) << (2 * (off & 31));
            stashantipodedense(off);
            newseen++;
            break;
          }
        }
      }
    }
  } else {
    for (int smoff = 0; smoff < symcoordsize; smoff++, symtab += nmoves) {
      ull off = sofar + smoff;
      if (mem[off >> 5] == 0xffffffffffffffffLL) {
        int acc = 31 - (off & 31);
        smoff += acc;
        symtab += acc * nmoves;
        continue;
      }
      int v = 3 & (mem[off >> 5] >> (2 * (off & 31)));
      if (v == seek) {
        for (int m = 0; m < nmoves; m++) {
          ull off2 = muld[m] + symtab[m];
          int v2 = 3 & (mem[off2 >> 5] >> (2 * (off2 & 31)));
          if (v2 == 3) {
            mem[off2 >> 5] -= (3LL - newv) << (2 * (off2 & 31));
            stashantipodedense(off2);
            newseen++;
          }
        }
      }
    }
  }
}
void recur(puzdef &pd, int at, int back, int seek, int newv, ull sofar,
           vector<ull> &muld) {
  if (at + numsym == (int)parts.size()) {
    innerloop(back, seek, newv, sofar, muld);
    return;
  }
  int sdpair = parts[at].second;
  setdef &sd = pd.setdefs[sdpair >> 1];
  vector<ull> muld2(nmoves);
  stacksetval p1(pd);
  stacksetval p2(pd);
  uchar *wmem = p1.dat;
  uchar *wmem2 = p2.dat;
  if (sdpair & 1) {
    ull sz = sd.llords;
    for (ull val = 0; val < sz; val++) {
      if (sd.oparity)
        indextoords2(wmem, val, sd.omod, sd.size);
      else
        indextoords(wmem, val, sd.omod, sd.size);
      for (int m = 0; m < nmoves; m++) {
        sd.mulo(wmem, pd.moves[movemap[m]].pos.dat + sd.off + sd.size, wmem2);
        if (sd.oparity)
          muld2[m] = ordstoindex(wmem2, sd.omod, sd.size - 1) + sz * muld[m];
        else
          muld2[m] = ordstoindex(wmem2, sd.omod, sd.size) + sz * muld[m];
      }
      recur(pd, at + 1, back, seek, newv, val + sofar * sz, muld2);
    }
  } else {
    ull sz = sd.llperms;
    for (ull val = 0; val < sz; val++) {
      if (sd.uniq) {
        if (sd.pparity)
          indextoperm2(wmem, val, sd.size);
        else
          indextoperm(wmem, val, sd.size);
      } else
        indextomperm(wmem, val, sd.cnts);
      for (int m = 0; m < nmoves; m++) {
        sd.mulp(wmem, pd.moves[movemap[m]].pos.dat + sd.off, wmem2);
        if (sd.uniq) {
          if (sd.pparity)
            muld2[m] = permtoindex2(wmem2, sd.size) + sz * muld[m];
          else
            muld2[m] = permtoindex(wmem2, sd.size) + sz * muld[m];
        } else
          muld2[m] = mpermtoindex(wmem2, sd.size) + sz * muld[m];
      }
      recur(pd, at + 1, back, seek, newv, val + sofar * sz, muld2);
    }
  }
}
void dotwobitgod2(puzdef &pd) {
  ull nlongs = (pd.llstates + 31) >> 5;
  ull memneeded = nlongs * 8;
  /*
   *   First, try to develop a strategy.
   */
  parts.clear();
  movemap.clear();
  for (int i = 0; i < (int)pd.moves.size(); i++)
    if (!quarter || pd.moves[i].cost == 1)
      movemap.push_back(i);
  nmoves = movemap.size();
  for (int i = 0; i < (int)pd.setdefs.size(); i++) {
    setdef &sd = pd.setdefs[i];
    if (!sd.dense)
      error("! we don't support dense packing of this puzzle yet");
    parts.push_back(make_pair(sd.llperms, i * 2));
    if (sd.llords > 1)
      parts.push_back(make_pair(sd.llords, i * 2 + 1));
  }
  sort(parts.begin(), parts.end());
  // how many parts should we use for the sym coord?
  numsym = 0;
  symcoordsize = 1;
  ull hicount = (maxmem - memneeded) / (4 * nmoves);
  while (numsym < (int)parts.size()) {
    ull tsymcoordsize = symcoordsize * parts[numsym].first;
    // never go past 32 bits, or past maxmem
    if (tsymcoordsize > 0xffffffffLL || tsymcoordsize > hicount)
      break;
    if (tsymcoordsize / symcoordgoal > symcoordgoal / symcoordsize)
      break;
    numsym++;
    symcoordsize = tsymcoordsize;
  }
  // can't split, fall back to simpler way
  if (numsym == 0) {
    dotwobitgod(pd);
    return;
  }
  cout << "Sizes [";
  for (int i = 0; i < (int)parts.size(); i++) {
    if (i)
      cout << " ";
    cout << parts[i].first;
    if (i + 1 == numsym)
      cout << "]";
  }
  cout << endl << flush;
  reverse(parts.begin(), parts.end());
  // consider adding support for shorts here for cache friendliness.
  symc = (unsigned int *)calloc(symcoordsize * nmoves, sizeof(unsigned int));
  if (symc == 0)
    error("! not enough memory");
  cout << "Making symcoord lookup table size " << symcoordsize << " x "
       << nmoves << flush;
  unsigned int *ss = symc;
  for (ll i = 0; i < symcoordsize; i++, ss += nmoves) {
    stacksetval p1(pd);
    stacksetval p2(pd);
    uchar *wmem = p1.dat;
    uchar *wmem2 = p2.dat;
    ull u = i;
    ull mul = 1;
    for (int j = parts.size() - 1; j + numsym >= (int)parts.size(); j--) {
      int sdpair = parts[j].second;
      setdef &sd = pd.setdefs[sdpair >> 1];
      if (sdpair & 1) {
        ull sz = sd.llords;
        ull val = u % sz;
        u /= sz;
        for (int m = 0; m < nmoves; m++) {
          if (sd.oparity)
            indextoords2(wmem, val, sd.omod, sd.size);
          else
            indextoords(wmem, val, sd.omod, sd.size);
          sd.mulo(wmem, pd.moves[movemap[m]].pos.dat + sd.off + sd.size, wmem2);
          if (sd.oparity)
            ss[m] += mul * ordstoindex(wmem2, sd.omod, sd.size - 1);
          else
            ss[m] += mul * ordstoindex(wmem2, sd.omod, sd.size);
        }
        mul *= sz;
      } else {
        ull sz = sd.llperms;
        ull val = u % sz;
        u /= sz;
        for (int m = 0; m < nmoves; m++) {
          if (sd.uniq) {
            if (sd.pparity)
              indextoperm2(wmem, val, sd.size);
            else
              indextoperm(wmem, val, sd.size);
          } else
            indextomperm(wmem, val, sd.cnts);
          sd.mulp(wmem, pd.moves[movemap[m]].pos.dat + sd.off, wmem2);
          if (sd.uniq) {
            if (sd.pparity)
              ss[m] += mul * permtoindex2(wmem2, sd.size);
            else
              ss[m] += mul * permtoindex(wmem2, sd.size);
          } else
            ss[m] += mul * mpermtoindex(wmem2, sd.size);
        }
        mul *= sz;
      }
    }
  }
  cout << " in " << duration() << endl << flush;
  mem = (ull *)malloc(memneeded);
  if (mem == 0)
    error("! not enough memory");
  memset(mem, -1, memneeded);
  stacksetval p1(pd), p2(pd);
  pd.assignpos(p1, pd.solved);
  ull off = densepack_ordered(pd, p1);
  mem[off >> 5] -= 3LL << (2 * (off & 31));
  cnts.clear();
  cnts.push_back(1);
  ull tot = 1;
  for (int d = 0;; d++) {
    resetantipodes();
    cout << "Dist " << d << " cnt " << cnts[d] << " tot " << tot << " in "
         << duration() << endl
         << flush;
    if (cnts[d] == 0 || (pd.logstates <= 62 && tot == pd.llstates))
      break;
    newseen = 0;
    // don't be too aggressive, because we might see parity and this might slow
    // things down dramatically; only go backwards after more than 50% full.
    int back = (pd.logstates <= 62 && tot * 2 > pd.llstates);
    int seek = d % 3;
    int newv = (d + 1) % 3;
    vector<ull> muld(nmoves);
    recur(pd, 0, back, seek, newv, 0, muld);
    cnts.push_back(newseen);
    tot += newseen;
  }
  showantipodesdense(pd, 1);
}
/*
 *   This version just builds a move table for each individual set,
 *   individually, then does a Cartesian product.  We don't depend on
 *   dense encoding nor do we depend on state size estimates; we just
 *   calculate the full reachability of each set.  We do assume that
 *   each packed setdef fits in a long though.
 */
struct levelinfo {
  ull wbase, cnt, startbit;
};
struct setinfo {
  ull numstates, mult;
  unsigned int *movetable;
};
/*
 *   rdv=0 means we are going backwards.
 */
ull recur(const vector<setinfo> &mt, vector<ull> &offsets, int at, int rdv,
          int wrv, int nmoves, ull roff, ull *bits, int back) {
  // cout << "Recur sees" ;
  // for (auto v: offsets) cout << " " << v ;
  // cout << endl ;
  ull r = 0;
  if (at == (int)mt.size() - 1) {
    ull wid = mt[at].mult >> 5;
    roff *= wid;
    for (int mv = 0; mv < nmoves; mv++)
      offsets[at * nmoves + mv] *= mt[at].mult;
    if (back) { // backwards
      ull xormask = 0xffffffffffffffffLL;
      for (ull o = 0; o < wid; o++) {
        ull rd = bits[roff + o] ^ xormask;
        rd = rd & (rd >> 1) & 0x5555555555555555LL;
        // cout << "Rd is " << rd << endl ;
        while (rd) {
          int o2 = (ffsll(rd) - 1);
          rd &= ~(1LL << o2);
          ull ofr = (o << 5) + (o2 >> 1);
          for (int mv = 0; mv < nmoves; mv++) {
            // cout << "Inmv sees" ;
            // for (auto v: offsets) cout << " " << v ;
            // cout << endl ;
            ull woff =
                offsets[at * nmoves + mv] + mt[at].movetable[ofr * nmoves + mv];
            // cout << "Woff is " << woff << " from " << at*nmoves+mv << " " <<
            // offsets[at*nmoves+mv] << " " << mt[at].movetable[ofr*nmoves+mv]
            // << endl ;
            ull wbits = bits[woff >> 5];
            if ((int)(((wbits >> ((woff & 31) * 2)) & 3)) == rdv) {
              bits[roff + o] += ((ull)wrv) << o2;
              r++;
              break;
            }
          }
        }
      }
    } else { // forwards
      ull xormask = ((ull)(3 - rdv)) * 0x5555555555555555LL;
      for (ull o = 0; o < wid; o++) {
        ull rd = bits[roff + o] ^ xormask;
        rd = rd & (rd >> 1) & 0x5555555555555555LL;
        // cout << "Rd is " << rd << endl ;
        while (rd) {
          int o2 = (ffsll(rd) - 1);
          rd &= ~(1LL << o2);
          ull ofr = (o << 5) + (o2 >> 1);
          for (int mv = 0; mv < nmoves; mv++) {
            // cout << "Inmv sees" ;
            // for (auto v: offsets) cout << " " << v ;
            // cout << endl ;
            ull woff =
                offsets[at * nmoves + mv] + mt[at].movetable[ofr * nmoves + mv];
            // cout << "Woff is " << woff << " from " << at*nmoves+mv << " " <<
            // offsets[at*nmoves+mv] << " " << mt[at].movetable[ofr*nmoves+mv]
            // << endl ;
            ull wbits = bits[woff >> 5];
            if (((wbits >> ((woff & 31) * 2)) & 3) == 0) {
              bits[woff >> 5] += ((ull)wrv) << ((woff & 31) * 2);
              r++;
            }
          }
        }
      }
    }
  } else {
    for (ull v = 0; v < mt[at].numstates; v++) {
      for (int mv = 0; mv < nmoves; mv++) {
        offsets[(at + 1) * nmoves + mv] =
            offsets[at * nmoves + mv] * mt[at].mult +
            mt[at].movetable[v * nmoves + mv];
        // cout << "Read " << offsets[at*nmoves+mv] << " from movetable " <<
        // mt[at].movetable[v*nmoves+mv] << " gives " <<
        // offsets[(at+1)*nmoves+mv] << endl ;
      }
      r += recur(mt, offsets, at + 1, rdv, wrv, nmoves, roff * mt[at].mult + v,
                 bits, back);
    }
  }
  return r;
}
loosetype *sortuniq(loosetype *s_2, loosetype *s_1, loosetype *beg,
                    loosetype *end, int temp, loosetype *lim, int looseper);
static inline int compare(const void *a_, const void *b_, int looseper);
void dotwobitgod3(puzdef &pd) {
  movemap.clear();
  vector<int> imoves;
  for (int i = 0; i < (int)pd.moves.size(); i++)
    if (!quarter || pd.moves[i].cost == 1) {
      movemap.push_back(i);
      imoves.push_back(pd.invmove(i));
    }
  nmoves = movemap.size();
  vector<setinfo> movetables;
  for (int i = 0; i < (int)pd.setdefs.size(); i++) {
    cout << "Freeing earlier work in " << duration() << endl;
    setdef &sd = pd.setdefs[i];
    int lp = looseperone(pd, i, 0);
    cout << "Calculating move table for setdef " << sd.name << " lp " << lp
         << endl;
    vector<loosetype> workarr;
    stacksetval p1(pd), p2(pd);
    pd.assignpos(p1, pd.solved);
    while ((int)workarr.size() < lp)
      workarr.push_back(0);
    // first, calculate how many positions
    loosepackone(pd, p1, i, &(workarr[0]), 0, 0);
    pd.assignpos(p2, p1);
    vector<levelinfo> li;
    li.push_back({0, 1, 0});
    ull orbase = 0;
    for (int rd = 0;; rd++) {
      ull rbase = li[rd].wbase;
      ull rcnt = li[rd].cnt;
      cout << "At dist " << rd << " see " << rcnt << endl;
      ull wbase = rbase + rcnt;
      ull wcnt = 0;
      for (ull j = 0; j < rcnt; j++) {
        looseunpackone(pd, p1, i, &(workarr[(rbase + j) * lp]));
        while (workarr.size() < (wbase + wcnt + nmoves) * lp)
          workarr.push_back(0);
        for (int mvi = 0; mvi < nmoves; mvi++) {
          pd.mul(p1, pd.moves[movemap[mvi]].pos, p2);
          auto wptr = &(workarr[(wbase + wcnt) * lp]);
          loosepackone(pd, p2, i, wptr, 0, 0);
          wcnt++;
        }
      }
      auto s2ptr = &(workarr[orbase * lp]);
      auto s1ptr = &(workarr[rbase * lp]);
      auto begptr = &(workarr[wbase * lp]);
      auto endptr = &(workarr[(wbase + wcnt) * lp]);
      auto nendptr = sortuniq(s2ptr, s1ptr, begptr, endptr, 0, endptr, lp);
      wcnt = (nendptr - begptr) / lp;
      li.push_back({wbase, wcnt, 0});
      if (wcnt == 0) {
        cout << "Saw " << wbase << " required "
             << workarr.size() * sizeof(loosetype) << " bytes in " << duration()
             << endl;
        break;
      }
      orbase = rbase;
    }
    ull wbase = li[li.size() - 1].wbase;
    if (wbase >= 4000000000LL)
      error("! too many states from this one set");
    for (int rd = 0; rd < (int)li.size(); rd++) {
      ull b = 1;
      while (b <= li[rd].cnt)
        b += b;
      li[rd].startbit = b;
    }
    unsigned int *mt = (unsigned int *)calloc(wbase, nmoves * sizeof(int));
    vector<loosetype> lptmp(lp);
    int lord[3];
    for (int rd = 0; rd < (int)li.size(); rd++) {
      int nordn = 0;
      for (int pr = rd - 1; pr <= rd + 1; pr++)
        if (pr >= 0 && pr < (int)li.size())
          lord[nordn++] = pr;
      ll rbase = li[rd].wbase;
      ll rcnt = li[rd].cnt;
      for (ll j = 0; j < rcnt; j++) {
        looseunpackone(pd, p1, i, &(workarr[(rbase + j) * lp]));
        for (int mvi = 0; mvi < nmoves; mvi++) {
          if (mt[(li[rd].wbase + j) * nmoves + mvi])
            continue;
          pd.mul(p1, pd.moves[movemap[mvi]].pos, p2);
          auto wptr = &(lptmp[0]);
          loosepackone(pd, p2, i, wptr, 0, 0);
          ll found = 0xfffffffffffffffLL;
          for (int pri = 0; pri < nordn; pri++) {
            int pr = lord[pri];
            ull loc = 0;
            for (ull b = li[pr].startbit; b; b >>= 1) {
              if (loc + b < li[pr].cnt &&
                  compare(&(workarr[(li[pr].wbase + loc + b) * lp]), wptr,
                          lp) <= 0)
                loc += b;
            }
            if (loc < li[pr].cnt &&
                compare(&(workarr[(li[pr].wbase + loc) * lp]), wptr, lp) == 0) {
              found = loc + li[pr].wbase;
              if (pri > 0)
                swap(lord[pri], lord[pri >> 1]);
              break;
            }
          }
          if (found == 0xfffffffffffffffLL)
            error("! did not find");
          mt[(li[rd].wbase + j) * nmoves + mvi] = found;
          mt[found * nmoves + imoves[mvi]] = li[rd].wbase + j;
        }
      }
    }
    movetables.push_back({wbase, wbase, mt});
    cout << "Move table built in " << duration() << endl;
  }
  cout << "Freeing earlier work in " << duration() << endl;
  // for the last one, ensure the multiplier is a multiple of 32
  setinfo &si = movetables[movetables.size() - 1];
  si.mult = (si.numstates + 31) & ~31LL;
  ull totsize = 1;
  for (auto &si : movetables) {
    ull ntotsize = totsize * (ull)si.mult;
    if (ntotsize / (ull)si.mult != totsize)
      error("! overflow in size calculation");
    totsize = ntotsize;
  }
  ull bytesize = (totsize + 3) >> 2;
  bytesize = (bytesize + 7) & ~7LL;
  if (bytesize > (ull)maxmem) {
    cerr << "Bytesize required is " << bytesize << endl;
    error("! requires too much RAM");
  }
  ull *bits = (ull *)calloc(bytesize >> 3, sizeof(ull));
  bits[0] = 1;
  vector<ull> offsets(nmoves * (1 + movetables.size()));
  ll totset = 0;
  ll bitsset = 1;
  ull levcnts[4];
  for (int i = 0; i < 4; i++)
    levcnts[i] = 0;
  levcnts[0] = totsize - 1;
  levcnts[1] = 1;
  for (int rd = 0;; rd++) {
    totset += bitsset;
    cout << "Dist " << rd << " " << bitsset << " " << totset << " in "
         << duration() << endl;
    int rdv = (rd % 3) + 1;
    int wrv = (rdv % 3) + 1;
    for (int i = 0; i < nmoves; i++)
      offsets[i] = 0;
    if (levcnts[rdv] < levcnts[0])
      bitsset = recur(movetables, offsets, 0, rdv, wrv, nmoves, 0, bits, 0);
    else
      bitsset = recur(movetables, offsets, 0, rdv, wrv, nmoves, 0, bits, 1);
    if (bitsset == 0)
      break;
    levcnts[wrv] += bitsset;
    levcnts[0] -= bitsset;
  }
}
/*
 *   Dead code; uses C++ unordered_maps but this is likely too slow.
 *
void dotwobitgod4(puzdef &pd) {
  movemap.clear();
  for (int i = 0; i < (int)pd.moves.size(); i++)
    if (!quarter || pd.moves[i].cost == 1)
      movemap.push_back(i);
  nmoves = movemap.size();
  vector<int *> movetables ;
  for (int i = 0; i < (int)pd.setdefs.size(); i++) {
    cout << "Freeing earlier work in " << duration() << endl ;
    setdef &sd = pd.setdefs[i];
    cout << "Calculating move table for setdef " << sd.name << endl;
    unordered_map<ll, int> coordlookups;
    stacksetval p1(pd), p2(pd);
    pd.assignpos(p1, pd.solved);
    vector<ull> q;
    // first, calculate how many positions
    ull st = loosepackone(pd, p1, i, 0, 0);
    coordlookups[st] = 0;
    q.push_back(st);
    for (int qg=0; qg<(int)q.size(); qg++) {
      ull src = q[qg];
      looseunpackone(pd, p1, i, src);
      for (int mvi=0; mvi<nmoves; mvi++) {
        pd.mul(p1, pd.moves[movemap[mvi]].pos, p2);
        ull dst = loosepackone(pd, p2, i, 0, 0);
        if (coordlookups.find(dst) == coordlookups.end()) {
          coordlookups[dst] = q.size();
          q.push_back(dst);
        }
      }
    }
    cout << "Found " << q.size() << " elements; building move table in "
         << duration() << endl;
    int *mt = (int *)calloc((int)q.size(), nmoves*sizeof(int)) ;
    for (ll qg=0; qg<(int)q.size(); qg++) {
      ull src = q[qg];
      looseunpackone(pd, p1, i, src);
      for (int mvi=0; mvi<nmoves; mvi++) {
        pd.mul(p1, pd.moves[movemap[mvi]].pos, p2);
        ull dst = loosepackone(pd, p2, i, 0, 0);
        mt[qg*nmoves+mvi] = coordlookups[dst] ;
      }
    }
    movetables.push_back(mt) ;
    cout << "Move table built in " << duration() << endl ;
  }
  cout << "Freeing earlier work in " << duration() << endl ;
}
 */
static inline int compare(const void *a_, const void *b_, int looseper) {
  loosetype *a = (loosetype *)a_;
  loosetype *b = (loosetype *)b_;
  for (int i = 0; i < looseper; i++)
    if (a[i] != b[i])
      return (a[i] < b[i] ? -1 : 1);
  return 0;
}
static int qsortlooseper;
static inline int qsortcompare(const void *a_, const void *b_) {
  return compare(a_, b_, qsortlooseper);
}
const int SHIFT = 10;
const int BUCKETS = 1 << SHIFT;
const int SPLIT = 32;
template <typename T> int extract(const T &a) { return a[0] >> (32 - SHIFT); }
#ifdef USE_PTHREADS
static int wi;
#endif
static ll beg[SPLIT], endb[SPLIT];
static pair<ll, int> bysize[SPLIT];
template <typename T> void tmqsort(T *a, ll n) {
  if (n < 4096) {
    sort(a, a + n);
    return;
  }
  ll cnts[BUCKETS];
  for (int i = 0; i < BUCKETS; i++)
    cnts[i] = 0;
  for (ll i = 0; i < n; i++)
    cnts[extract(a[i])]++;
  int split[BUCKETS];
  ll cnts2[SPLIT];
  ll rem = n;
  ll goal = (2 * n + SPLIT) / (2 * SPLIT);
  int at = 0;
  for (int i = 0; i < SPLIT; i++)
    cnts2[i] = 0;
  for (int i = 0; i < BUCKETS; i++) {
    if (at + 1 < SPLIT && cnts2[at] + cnts[i] - goal > goal - cnts2[at]) {
      rem -= cnts2[at];
      goal = (2 * rem + (SPLIT - at)) / (2 * (SPLIT - at));
      at++;
    }
    split[i] = at;
    cnts2[at] += cnts[i];
  }
  ll s = 0;
  for (int i = 0; i < SPLIT; i++) {
    beg[i] = s;
    s += cnts2[i];
    endb[i] = s;
  }
  for (int b = 0; b < SPLIT; b++) {
    for (ll i = beg[b]; i < endb[b]; i++) {
      while (1) {
        int buck = split[extract(a[i])];
        if (buck == b)
          break;
        swap(a[i], a[beg[buck]++]);
      }
    }
  }
  for (int i = 0; i < SPLIT; i++)
    bysize[i] = {-cnts2[i], i};
  sort(bysize, bysize + SPLIT);
  s = 0;
  for (int i = 0; i < SPLIT; i++) {
    beg[i] = s;
    s += cnts2[i];
  }
#ifdef USE_PTHREADS
  wi = 0;
  auto worker = [](void *ap) -> void * {
    T *a = (T *)ap;
    while (1) {
      int w = -1;
      get_global_lock();
      if (wi < SPLIT)
        w = wi++;
      release_global_lock();
      if (w < 0)
        return (void *)0;
      int b = bysize[w].second;
      sort(a + beg[b], a + endb[b]);
    }
  };
  for (int i = 0; i < numthreads; i++)
    spawn_thread(i, worker, a);
  for (int i = 0; i < numthreads; i++)
    join_thread(i);
#else
  for (int i = 0; i < SPLIT; i++) {
    int b = bysize[i].second;
    sort(a + beg[b], a + endb[b]);
  }
#endif
}
void mqsort(void *beg, ll numel, int looseper, ll sz) {
  switch (looseper) {
  case 1:
    tmqsort((array<loosetype, 1> *)beg, numel);
    break;
  case 2:
    tmqsort((array<loosetype, 2> *)beg, numel);
    break;
  case 3:
    tmqsort((array<loosetype, 3> *)beg, numel);
    break;
  case 4:
    tmqsort((array<loosetype, 4> *)beg, numel);
    break;
  case 5:
    tmqsort((array<loosetype, 5> *)beg, numel);
    break;
  case 6:
    tmqsort((array<loosetype, 6> *)beg, numel);
    break;
  case 7:
    tmqsort((array<loosetype, 7> *)beg, numel);
    break;
  case 8:
    tmqsort((array<loosetype, 8> *)beg, numel);
    break;
  default:
    qsortlooseper = looseper;
    qsort(beg, numel, sz, qsortcompare);
  }
}
loosetype *sortuniq(loosetype *s_2, loosetype *s_1, loosetype *beg,
                    loosetype *end, int temp, loosetype *lim, int looseper) {
  size_t numel = (end - beg) / looseper;
  if (verbose > 1 || temp)
    cout << "Created " << numel << " elements in " << duration() << endl
         << flush;
  mqsort(beg, numel, looseper, looseper * sizeof(loosetype));
  if (verbose > 1)
    cout << "Sorted " << flush;
  loosetype *s_0 = beg;
  loosetype *w = beg;
  loosetype *r_2 = s_2;
  loosetype *r_1 = s_1;
  while (beg < end) {
    if (beg + looseper >= end || compare(beg, beg + looseper, looseper)) {
      while (r_2 < s_1 && compare(beg, r_2, looseper) > 0)
        r_2 += looseper;
      if (r_2 >= s_1 || compare(beg, r_2, looseper)) {
        while (r_1 < s_0 && compare(beg, r_1, looseper) > 0)
          r_1 += looseper;
        if (r_1 >= s_0 || compare(beg, r_1, looseper)) {
          memcpy(w, beg, looseper * sizeof(loosetype));
          w += looseper;
        }
      }
    }
    beg += looseper;
  }
  if (verbose > 1 || temp)
    cout << "to " << (w - s_0) / looseper << " in " << duration() << endl
         << flush;
  if (temp && (w + looseper - s_0) >= (lim - s_0) * 95 / 100)
    error("! out of memory");
  return w;
}
static loosetype *reader, *writer, *lim, *levend, *s_1, *s_2;
#ifdef USE_PTHREADS
/*
 *   Basic code for doing a section of the input to a buffer.
 *   Returns the number of positions written.  First, the
 *   version without symmetry.
 */
static int doarraygodchunk(const puzdef *pd, loosetype *reader,
                           loosetype *writer, int cnt) {
  int r = 0;
  const loosetype *levend = reader + cnt * looseper;
  stacksetval p1(*pd), p2(*pd), p3(*pd);
  for (loosetype *pr = reader; pr < levend; pr += looseper) {
    looseunpack(*pd, p1, pr);
    for (int i = 0; i < (int)pd->moves.size(); i++) {
      if (quarter && pd->moves[i].cost > 1)
        continue;
      pd->mul(p1, pd->moves[i].pos, p2);
      if (!pd->legalstate(p2))
        continue;
      loosepack(*pd, p2, writer);
      writer += looseper;
      r++;
    }
  }
  return r;
}
/*
 *   Next the version with symmetry.
 */
static int doarraygodsymchunk(const puzdef *pd, loosetype *reader,
                              loosetype *writer, int cnt) {
  int r = 0;
  const loosetype *levend = reader + cnt * looseper;
  stacksetval p1(*pd), p2(*pd), p3(*pd), p4(*pd);
  for (loosetype *pr = reader; pr < levend; pr += looseper) {
    looseunpack(*pd, p1, pr);
    for (int doinv = 0; doinv < 2; doinv++) {
      for (int i = 0; i < (int)pd->moves.size(); i++) {
        if (quarter && pd->moves[i].cost > 1)
          continue;
        pd->mul(p1, pd->moves[i].pos, p2);
        if (!pd->legalstate(p2))
          continue;
        int sym;
        if (pd->invertible()) {
          sym = slowmodm2inv(*pd, p2, p3, p4);
          if ((sym & MODINV_FORWARD) == 0)
            continue;
          sym &= MODINV_CNTMASK;
        } else
          sym = slowmodm2(*pd, p2, p3);
        loosepack(*pd, p3, writer, 0, 1 + (sym > 1));
        writer += looseper;
        r++;
      }
      if (!pd->invertible())
        break;
      if (doinv == 0) {
        pd->inv(p1, p2);
        pd->assignpos(p1, p2);
      }
    }
  }
  return r;
}
const size_t BUFSIZE = 1 << 18;
static ll maxcnt, wavail;
void setupgwork(const puzdef &pd) {
  if (pd.invertible())
    maxcnt = BUFSIZE / (sizeof(loosetype) * looseper * 2 * pd.moves.size());
  else
    maxcnt = BUFSIZE / (sizeof(loosetype) * looseper * pd.moves.size());
  maxcnt = min(maxcnt, (ll)(1 + (levend - reader) / (looseper * numthreads)));
  wavail = (lim - writer) / looseper;
}
static struct gworker {
  void init(const puzdef *_pd, int _usesym) {
    pd = _pd;
    usesym = _usesym;
    if (buf == 0)
      buf = (loosetype *)malloc(BUFSIZE);
  }
  int getwork(ll &cnt) {
    cnt = maxcnt;
    ll rlim = (levend - reader) / looseper;
    ll wlim = wavail / pd->moves.size();
    if (pd->invertible())
      wlim = wavail / (2 * pd->moves.size());
    cnt = min(cnt, min(rlim, wlim));
    if (cnt <= 0)
      return 0;
    if (pd->invertible())
      wavail -= cnt * 2 * pd->moves.size();
    else
      wavail -= cnt * pd->moves.size();
    reader += cnt * looseper;
    return 1;
  }
  void work() {
    while (1) {
      get_global_lock();
      loosetype *reader = ::reader;
      ll cnt;
      int r = getwork(cnt);
      release_global_lock();
      if (r <= 0)
        return;
      ll ncnt = 0;
      if (usesym)
        ncnt = doarraygodsymchunk(pd, reader, buf, cnt);
      else
        ncnt = doarraygodchunk(pd, reader, buf, cnt);
      get_global_lock();
      if (pd->invertible())
        wavail += 2 * pd->moves.size() * cnt - ncnt;
      else
        wavail += pd->moves.size() * cnt - ncnt;
      memcpy(writer, buf, sizeof(loosetype) * looseper * ncnt);
      writer += looseper * ncnt;
      release_global_lock();
    }
  }
  loosetype *buf;
  const puzdef *pd;
  int usesym;
} gworkers[MAXTHREADS];
static void *dogodwork(void *o) {
  gworker *gw = (gworker *)o;
  gw->work();
  return 0;
}
#endif
/*
 *   God's algorithm as far as we can go, using fixed-length byte chunks
 *   packed (but not densely) and sorting.
 */
void doarraygod(const puzdef &pd) {
  ull memneeded = maxmem;
  loosetype *mem = (loosetype *)malloc(memneeded);
  if (mem == 0)
    error("! not enough memory");
  stacksetval p1(pd), p2(pd), p3(pd);
  pd.assignpos(p1, pd.solved);
  loosepack(pd, p1, mem);
  cnts.clear();
  cnts.push_back(1);
  ull tot = 1;
  lim = mem + memneeded / (sizeof(loosetype) * looseper) * looseper;
  reader = mem;
  writer = mem + looseper;
  s_1 = mem;
  s_2 = mem;
  for (int d = 0;; d++) {
    cout << "Dist " << d << " cnt " << cnts[d] << " tot " << tot << " in "
         << duration() << endl
         << flush;
    if (cnts[d] == 0 || (pd.logstates <= 62 && tot == pd.llstates))
      break;
    ull newseen = 0;
    levend = writer;
#ifdef USE_PTHREADS
    if (numthreads > 1) {
      while (1) {
        setupgwork(pd);
        for (int i = 0; i < numthreads; i++)
          gworkers[i].init(&pd, 0);
        for (int i = 0; i < numthreads; i++)
          spawn_thread(i, dogodwork, gworkers + i);
        for (int i = 0; i < numthreads; i++)
          join_thread(i);
        if (reader == levend)
          break;
        writer = sortuniq(s_2, s_1, levend, writer, 1, lim, looseper);
      }
    } else {
#endif
      for (loosetype *pr = reader; pr < levend; pr += looseper) {
        looseunpack(pd, p1, pr);
        for (int i = 0; i < (int)pd.moves.size(); i++) {
          if (quarter && pd.moves[i].cost > 1)
            continue;
          pd.mul(p1, pd.moves[i].pos, p2);
          if (!pd.legalstate(p2))
            continue;
          loosepack(pd, p2, writer);
          writer += looseper;
          if (writer + looseper >= lim)
            writer = sortuniq(s_2, s_1, levend, writer, 1, lim, looseper);
        }
      }
#ifdef USE_PTHREADS
    }
#endif
    writer = sortuniq(s_2, s_1, levend, writer, 0, lim, looseper);
    newseen = (writer - levend) / looseper;
    cnts.push_back(newseen);
    tot += newseen;
    s_2 = s_1;
    s_1 = levend;
    reader = levend;
    if (s_2 != mem) {
      ll drop = s_2 - mem;
      memmove(mem, s_2, (writer - s_2) * sizeof(loosetype));
      s_1 -= drop;
      s_2 -= drop;
      reader -= drop;
      writer -= drop;
      levend -= drop;
    }
  }
  if (s_1 == writer) {
    showantipodes(pd, s_2, s_1);
  } else {
    showantipodes(pd, s_1, writer);
  }
}
/*
 *   God's algorithm as far as we can go, using fixed-length byte chunks
 *   packed (but not densely) and sorting, but this time using a recursive
 *   enumeration process rather than using a frontier.
 */
void dorecurgod(const puzdef &pd, int togo, int sp, int st) {
  if (togo == 0) {
    loosepack(pd, posns[sp], writer);
    writer += looseper;
    if (writer + looseper >= lim)
      writer = sortuniq(s_2, s_1, levend, writer, 1, lim, looseper);
    return;
  }
  ull mask = canonmask[st];
  const vector<int> &ns = canonnext[st];
  for (int m = 0; m < (int)pd.moves.size(); m++) {
    const moove &mv = pd.moves[m];
    if ((mask >> mv.cs) & 1)
      continue;
    pd.mul(posns[sp], mv.pos, posns[sp + 1]);
    if (!pd.legalstate(posns[sp + 1]))
      continue;
    dorecurgod(pd, togo - 1, sp + 1, ns[mv.cs]);
  }
}
void doarraygod2(const puzdef &pd) {
  ull memneeded = maxmem;
  loosetype *mem = (loosetype *)malloc(memneeded);
  if (mem == 0)
    error("! not enough memory");
  cnts.clear();
  ull tot = 0;
  lim = mem + memneeded / (sizeof(loosetype) * looseper) * looseper;
  reader = mem;
  writer = mem;
  s_1 = mem;
  s_2 = mem;
  movehist.clear();
  posns.clear();
  for (int d = 0;; d++) {
    resetantipodes();
    while ((int)posns.size() <= d + 1) {
      posns.push_back(allocsetval(pd, pd.solved));
      movehist.push_back(-1);
    }
    pd.assignpos(posns[0], pd.solved);
    ull newseen = 0;
    levend = writer;
    dorecurgod(pd, d, 0, 0);
    writer = sortuniq(s_2, s_1, levend, writer, 0, lim, looseper);
    newseen = (writer - levend) / looseper;
    cnts.push_back(newseen);
    tot += newseen;
    cout << "Dist " << d << " cnt " << cnts[d] << " tot " << tot << " in "
         << duration() << endl
         << flush;
    if (cnts[d] > 0)
      stashantipodesloose(levend, writer);
    if (cnts[d] == 0 || (pd.logstates <= 62 && tot == pd.llstates))
      break;
    if (levend != s_2)
      mqsort(s_2, (levend - s_2) / looseper, looseper,
             looseper * sizeof(loosetype));
    s_1 = levend;
    reader = levend;
  }
  showantipodesloose(pd);
}
ull calcsymseen(const puzdef &pd, loosetype *p, ull cnt, vector<int> *rotmul) {
  int symoff = basebits / (sizeof(loosetype) * 8);
  loosetype symbit = (1LL << (basebits & ((sizeof(loosetype) * 8) - 1)));
  int rots = pd.rotgroup.size();
  if (pd.invertible())
    rots *= 2;
  ull r = cnt * rots;
  stacksetval p1(pd), p2(pd), p3(pd);
  for (ull i = 0; i < cnt; i++, p += looseper) {
    if (p[symoff] & symbit) {
      looseunpack(pd, p1, p);
      int sym;
      if (pd.invertible())
        sym = slowmodm2inv(pd, p1, p2, p3) & MODINV_CNTMASK;
      else
        sym = slowmodm2(pd, p1, p2);
      if ((*rotmul)[sym] == 0 || (*rotmul)[sym] > rots) {
        error("! bad symmetry calculation");
      }
      r += (*rotmul)[sym] - rots;
    }
  }
  return r;
}
#ifdef USE_PTHREADS
static struct csworker {
  void init(const puzdef *_pd, loosetype *_start, ll _cnt,
            vector<int> *_rotmul) {
    pd = _pd;
    start = _start;
    cnt = _cnt;
    rotmul = _rotmul;
    tot = 0;
  }
  void work() {
    ull t = calcsymseen(*pd, start, cnt, rotmul);
    get_global_lock();
    tot = t;
    release_global_lock();
  }
  const puzdef *pd;
  loosetype *start;
  ll cnt, tot;
  vector<int> *rotmul;
} csworkers[MAXTHREADS];
static void *docswork(void *o) {
  csworker *csw = (csworker *)o;
  csw->work();
  return 0;
}
#endif
/*
 *   Given a sequence of loosepacked states, calculate the total number
 *   of states represented by these, unpacking the symmetry.
 */
ull calcsymseen(const puzdef &pd, loosetype *p, ull cnt) {
  int rots = pd.rotgroup.size();
  if (pd.invertible())
    rots *= 2;
  vector<int> rotmul(rots + 1);
  for (int i = 1; i * i <= rots; i++)
    if (rots % i == 0) {
      rotmul[i] = rots / i;
      rotmul[rots / i] = i;
    }
#ifdef USE_PTHREADS
  if (numthreads > 1) {
    for (int i = 0; i < numthreads; i++) {
      ull me = cnt / (numthreads - i);
      csworkers[i].init(&pd, p, me, &rotmul);
      cnt -= me;
      p += me * looseper;
    }
    for (int i = 0; i < numthreads; i++)
      spawn_thread(i, docswork, csworkers + i);
    for (int i = 0; i < numthreads; i++)
      join_thread(i);
    ull r = 0;
    for (int i = 0; i < numthreads; i++)
      r += csworkers[i].tot;
    return r;
  } else {
#endif
    return calcsymseen(pd, p, cnt, &rotmul);
#ifdef USE_PTHREADS
  }
#endif
}
/*
 *   God's algorithm using symmetry reduction.
 */
void doarraygodsymm(const puzdef &pd) {
  ull memneeded = maxmem;
  loosetype *mem = (loosetype *)malloc(memneeded);
  if (mem == 0)
    error("! not enough memory");
  stacksetval p1(pd), p2(pd), p3(pd), p4(pd);
  pd.assignpos(p2, pd.solved);
  int sym = slowmodm2(pd, p2, p1);
  loosepack(pd, p1, mem, 0, 1 + (sym > 1));
  cnts.clear();
  cnts.push_back(1);
  scnts.clear();
  scnts.push_back(1);
  ull tot = 1;
  ull stot = 1;
  lim = mem + memneeded / (sizeof(loosetype) * looseper) * looseper;
  reader = mem;
  writer = mem + looseper;
  loosetype *s_1 = mem;
  loosetype *s_2 = mem;
  int usesym = 1;
  if (pd.invertible())
    usesym++;
  for (int d = 0;; d++) {
    cout << "Dist " << d << " cnt " << cnts[d] << " tot " << tot << " scnt "
         << scnts[d] << " stot " << stot << " in " << duration() << endl
         << flush;
    if (cnts[d] == 0 || (pd.logstates <= 62 && tot == pd.llstates))
      break;
    ull newseen = 0;
    levend = writer;
#ifdef USE_PTHREADS
    if (numthreads > 1) {
      while (1) {
        setupgwork(pd);
        for (int i = 0; i < numthreads; i++)
          gworkers[i].init(&pd, usesym);
        for (int i = 0; i < numthreads; i++)
          spawn_thread(i, dogodwork, gworkers + i);
        for (int i = 0; i < numthreads; i++)
          join_thread(i);
        if (reader == levend)
          break;
        writer = sortuniq(s_2, s_1, levend, writer, 1, lim, looseper);
      }
    } else {
#endif
      for (loosetype *pr = reader; pr < levend; pr += looseper) {
        looseunpack(pd, p1, pr);
        for (int doinv = 0; doinv < 2; doinv++) {
          for (int i = 0; i < (int)pd.moves.size(); i++) {
            if (quarter && pd.moves[i].cost > 1)
              continue;
            pd.mul(p1, pd.moves[i].pos, p2);
            if (!pd.legalstate(p2))
              continue;
            if (pd.invertible()) {
              sym = slowmodm2inv(pd, p2, p3, p4);
              if ((sym & MODINV_FORWARD) == 0)
                continue;
              sym &= MODINV_CNTMASK;
            } else
              sym = slowmodm2(pd, p2, p3);
            loosepack(pd, p3, writer, 0, 1 + (sym > 1));
            writer += looseper;
            if (writer + looseper >= lim)
              writer = sortuniq(s_2, s_1, levend, writer, 1, lim, looseper);
          }
          if (!pd.invertible())
            break;
          if (doinv == 0) {
            pd.inv(p1, p2);
            pd.assignpos(p1, p2);
          }
        }
      }
#ifdef USE_PTHREADS
    }
#endif
    writer = sortuniq(s_2, s_1, levend, writer, 0, lim, looseper);
    newseen = (writer - levend) / looseper;
    cnts.push_back(newseen);
    tot += newseen;
    ull newsseen = calcsymseen(pd, levend, newseen);
    scnts.push_back(newsseen);
    stot += newsseen;
    s_2 = s_1;
    s_1 = levend;
    reader = levend;
    if (s_2 != mem) {
      ll drop = s_2 - mem;
      memmove(mem, s_2, (writer - s_2) * sizeof(loosetype));
      s_1 -= drop;
      s_2 -= drop;
      reader -= drop;
      writer -= drop;
      levend -= drop;
    }
  }
  if (s_1 == writer) {
    showantipodes(pd, s_2, s_1);
  } else {
    showantipodes(pd, s_1, writer);
  }
}
static int forcearray;
static boolopt
    force("-F",
          "When running God's number searches, force the use of arrays and\n"
          "sorting rather than canonical sequences or bit arrays.",
          &forcearray);
static struct godcmd : cmd {
  godcmd()
      : cmd("-g", "Calculate the number of positions at each depth, as far as "
                  "memory\n"
                  "allows.  Print antipodal positions.") {}
  virtual void docommand(puzdef &pd) {
    int statesfit2 = pd.logstates <= 50 && ((ll)(pd.llstates >> 2)) <= maxmem;
    int statesfitsa =
        forcearray ||
        (pd.logstates <= 50 &&
         ((ll)(pd.llstates * sizeof(loosetype) * looseper) <= maxmem));
    if (0 && !forcearray) { // disable new experimental code
      cout << "Using twobit arrays and separate setdefs" << endl;
      dotwobitgod3(pd);
    } else if (!forcearray && statesfit2 && pd.canpackdense()) {
      cout << "Using twobit arrays." << endl;
      dotwobitgod2(pd);
    } else if (statesfitsa) {
      if (pd.rotgroup.size() > 1) {
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
} registermeg;
