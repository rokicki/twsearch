#include "cmdlineops.h"
#include "canon.h"
#include "index.h"
#include "parsemoves.h"
#include "prunetable.h"
#include "readksolve.h"
#include "rotations.h"
#include "shorten.h"
#include "solve.h"
#include "unrotate.h"
#include <iostream>
ll proclim = 1'000'000'000'000'000'000LL;
int compact;
int maxwrong;
void solvecmdline(puzdef &pd, const char *scr, generatingset *gs) {
  stacksetval p1(pd);
  pd.assignpos(p1, pd.solved);
  string noname;
  prunetable pt(pd, maxmem);
  vector<allocsetval> movelist = parsemovelist_generously(pd, scr);
  for (int i = 0; i < (int)movelist.size(); i++)
    domove(pd, p1, movelist[i]);
  solveit(pd, pt, noname, p1, gs);
}
void processscrambles(istream *f, puzdef &pd, generatingset *gs) {
  prunetable pt(pd, maxmem);
  processscrambles(f, pd, pt, gs);
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
void readfirstscramble(istream *f, puzdef &pd, setval sv) {
  string scramblename;
  ull checksum = 0;
  while (1) {
    vector<string> toks = getline(f, checksum);
    if (toks.size() == 0)
      break;
    if (toks[0] == "Scramble" || toks[0] == "StartState") {
      expect(toks, 2);
      scramblename = toks[1];
      allocsetval p =
          readposition(pd, 'S', f, checksum, toks[0] == "StartState");
      pd.assignpos(sv, p);
      return;
    } else if (toks[0] == "ScrambleAlg") {
      expect(toks, 2);
      scramblename = toks[1];
      pd.assignpos(sv, pd.solved);
      while (1) {
        toks = getline(f, checksum);
        if (toks.size() == 0)
          error("! early end of line while reading ScrambleAlg");
        if (toks[0] == "End")
          break;
        for (int i = 0; i < (int)toks.size(); i++)
          domove(pd, sv, findmove_generously(pd, toks[i]));
      }
      return;
    } else {
      error("! unsupported command in scramble file");
    }
  }
}
vector<loosetype> uniqwork;
set<vector<loosetype>> uniqseen;
void uniqit(const puzdef &pd, setval p, const char *s) {
  uniqwork.resize(looseper);
  loosepack(pd, p, &uniqwork[0]);
  if (uniqseen.find(uniqwork) == uniqseen.end()) {
    uniqseen.insert(uniqwork);
    cout << s << endl << flush;
    proclim--;
    if (proclim == 0)
      exit(0);
  }
}
void wrongit(const puzdef &pd, setval p, const char *s) {
  int t = pd.numwrong(p, pd.solved);
  if (t <= maxwrong) {
    cout << t << " " << s << endl << flush;
    proclim--;
    if (proclim == 0)
      exit(0);
  }
}
void uniqitsymm(const puzdef &pd, setval p, const char *s) {
  stacksetval pw(pd);
  slowmodmip(pd, p, pw);
  uniqwork.resize(looseper);
  loosepack(pd, pw, &uniqwork[0]);
  if (uniqseen.find(uniqwork) == uniqseen.end()) {
    uniqseen.insert(uniqwork);
    cout << s << endl << flush;
    proclim--;
    if (proclim == 0)
      exit(0);
  }
}
void invertit(const puzdef &pd, vector<int> &movelist, const char *) {
  if (movelist.size() == 0) {
    cout << " ";
  } else {
    for (int i = 0; i < (int)movelist.size(); i++) {
      cout << " ";
      int ind = movelist[movelist.size() - 1 - i];
      const moove *mv = 0;
      int o;
      if (ind < (int)pd.moves.size()) {
        mv = &pd.moves[ind];
        int b = mv->base;
        o = pd.basemoveorders[b];
        cout << pd.basemoves[b].name;
      } else {
        ind -= pd.moves.size();
        mv = &pd.expandedrotations[ind];
        int b = mv->base;
        o = pd.baserotorders[b];
        cout << pd.rotations[b].name;
      }
      int twist = (o - mv->twist) % o;
      if (twist < 1) {
        cout << "?";
      } else if (twist == 1) {
        // nothing
      } else if (twist + twist <= o) {
        cout << twist;
      } else if (twist + 1 == o) {
        cout << "'";
      } else {
        cout << (o - twist) << "'";
      }
    }
  }
  cout << endl;
}
void cancelit(const puzdef &pd, vector<int> &movelist, const char *) {
  if (movelist.size() == 0) {
    cout << " ";
  } else {
    auto res = cancelmoves(pd, movelist);
    for (auto mvind : res)
      cout << " " << pd.moves[mvind].name;
  }
  cout << endl;
}
void mergeit(const puzdef &pd, vector<int> &movelist, const char *) {
  if (movelist.size() == 0) {
    cout << " ";
  } else {
    auto res = canonicalize(pd, movelist);
    for (auto mvind : res)
      cout << " " << pd.moves[mvind].name;
  }
  cout << endl;
}
void shortenit(const puzdef &pd, vector<int> &movelist, const char *) {
  if (movelist.size() == 0) {
    cout << " ";
  } else {
    auto res = shorten(pd, movelist);
    for (auto mvind : res)
      if (mvind < (int)pd.moves.size())
        cout << " " << pd.moves[mvind].name;
      else
        cout << " " << pd.rotations[mvind - pd.moves.size()].name;
  }
  cout << endl;
}
void unrotateit(const puzdef &pd, vector<int> &movelist, const char *) {
  if (movelist.size() == 0) {
    cout << " ";
  } else {
    auto res = unrotate(pd, movelist);
    for (auto mvind : res)
      if (mvind < (int)pd.moves.size())
        cout << " " << pd.moves[mvind].name;
      else
        cout << " " << pd.rotations[mvind - pd.moves.size()].name;
  }
  cout << endl;
}
void symsit(const puzdef &pd, setval p, const char *s) {
  stacksetval p2(pd);
  int symval = slowmodm(pd, p, p2);
  cout << s << ": " << symval << endl;
}
void orderit(const puzdef &pd, setval p, const char *s) {
  stacksetval p2(pd), p3(pd);
  pd.assignpos(p2, pd.solved);
  pd.mul(p2, p, p3);
  int m = 1;
  while (1) {
    if (pd.comparepos(p3, pd.solved) == 0) {
      cout << m << " " << s << endl;
      return;
    }
    pd.mul(p3, p, p2);
    m++;
    if (pd.comparepos(p2, pd.solved) == 0) {
      cout << m << " " << s << endl;
      return;
    }
    pd.mul(p2, p, p3);
    m++;
  }
}
void emitcompact(int v) {
  if (v < 10)
    cout << v;
  else if (v < 36)
    cout << (char)('A' + v - 10);
  else
    cout << (char)('a' + v - 36);
}
void emitmp(const puzdef &pd, setval p, const char *, int fixmoves) {
  uchar *a = p.dat;
  if (compact) {
    if (fixmoves)
      cout << "CMOV ";
    else
      cout << "CPOS ";
  } else {
    if (fixmoves)
      cout << "Move noname" << endl;
    else
      cout << "Scramble noname" << endl;
  }
  for (int i = 0; i < (int)pd.setdefs.size(); i++) {
    const setdef &sd = pd.setdefs[i];
    int n = sd.size;
    if (compact) {
      int nn = sd.size * sd.omod;
      for (int i = 0; i < n; i++)
        if (a[i + n] >= sd.omod)
          error("! compact doesn't support orientation ? yet");
      for (int i = 0; i < n; i++) {
        int v = a[i] * sd.omod + a[i + n];
        if (nn <= 62) {
          emitcompact(v);
        } else if (nn <= 62 * 62) {
          emitcompact(v / 62);
          emitcompact(v % 62);
        } else {
          error("! state too large for compact");
        }
      }
    } else {
      cout << "   " << pd.setdefs[i].name << endl;
      cout << "  ";
      for (int i = 0; i < n; i++)
        cout << " " << (int)(a[i] + 1);
      cout << endl;
      if (sd.omod > 1) {
        cout << "   ";
        if (fixmoves) {
          for (int i = 0; i < n; i++)
            if (a[i + n] >= sd.omod)
              error("! moves don't support orientation ? yet");
          vector<int> newori(n);
          for (int i = 0; i < n; i++)
            newori[a[i]] = a[i + n];
          for (int i = 0; i < n; i++)
            cout << " " << newori[i];
        } else {
          for (int i = 0; i < n; i++)
            if (a[i + n] >= sd.omod)
              cout << " ?";
            else
              cout << " " << (int)(a[i + n]);
        }
        cout << endl;
      }
    }
    a += 2 * n;
  }
  if (compact) {
    cout << endl;
  } else {
    cout << "End" << endl;
  }
}
void emitmove(const puzdef &pd, setval p, const char *s) {
  emitmp(pd, p, s, 1);
}
void emitposition(const puzdef &pd, setval p, const char *s) {
  emitmp(pd, p, s, 0);
}
void showrandompos(const puzdef &pd) {
  stacksetval p1(pd), p2(pd);
  pd.assignpos(p1, pd.solved);
  for (int i = 0; i < 500; i++) {
    int mv = myrand(pd.moves.size());
    pd.mul(p1, pd.moves[mv].pos, p2);
    mv = myrand(pd.moves.size());
    pd.mul(p2, pd.moves[mv].pos, p1);
  }
  emitposition(pd, p1, 0);
}
// basic infrastructure for walking a set of sequences
int globalinputmovecount = 0;
void processlines(const puzdef &pd,
                  function<void(const puzdef &, setval, const char *)> f) {
  string s;
  stacksetval p1(pd);
  while (getline(cin, s)) {
    pd.assignpos(p1, pd.solved);
    vector<allocsetval> movelist = parsemovelist_generously(pd, s.c_str());
    //    vector<int> moveid = parsemovelist(pd, s.c_str()) ;
    globalinputmovecount = movelist.size();
    for (int i = 0; i < (int)movelist.size(); i++)
      domove(pd, p1, movelist[i]);
    f(pd, p1, s.c_str());
  }
}
void processlines2(const puzdef &pd,
                   function<void(const puzdef &, setval, const char *)> f) {
  string s;
  stacksetval p1(pd);
  while (getline(cin, s)) {
    pd.assignpos(p1, pd.id);
    vector<allocsetval> movelist = parsemovelist_generously(pd, s.c_str());
    //    vector<int> moveid = parsemovelist(pd, s.c_str()) ;
    globalinputmovecount = movelist.size();
    for (int i = 0; i < (int)movelist.size(); i++)
      domove(pd, p1, movelist[i]);
    f(pd, p1, s.c_str());
  }
}
void processlines3(
    const puzdef &pd,
    function<void(const puzdef &, vector<int> &moveids, const char *)> f) {
  string s;
  stacksetval p1(pd);
  while (getline(cin, s)) {
    pd.assignpos(p1, pd.solved);
    vector<int> moveid = parsemovelist(pd, s.c_str());
    globalinputmovecount = moveid.size();
    f(pd, moveid, s.c_str());
  }
}
void processlines4(
    const puzdef &pd,
    function<void(const puzdef &, vector<int> &moveids, const char *)> f) {
  string s;
  stacksetval p1(pd);
  while (getline(cin, s)) {
    pd.assignpos(p1, pd.solved);
    vector<int> moveid = parsemoveorrotationlist(pd, s.c_str());
    globalinputmovecount = moveid.size();
    f(pd, moveid, s.c_str());
  }
}
