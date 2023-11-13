#include "cmdlineops.h"
#include "canon.h"
#include "cmds.h"
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
struct proclimcmd : cmd {
  proclimcmd(const char *shortopt, const char *longopt, const char *docs)
      : cmd(shortopt, longopt, docs) {}
  virtual void parse_args(int *, const char ***argv) {
    const char *p = **argv + 2;
    if (*p) {
      proclim = atoll(p);
    }
  }
};
int compact;
static struct compactopt : boolopt {
  compactopt()
      : boolopt(0, "--compact",
                "Print and parse positions on standard input and output\n"
                " in a one-line compact format.",
                &compact) {}
} registercompactopt;
ll maxwrong;
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
static struct uniqcmd : proclimcmd {
  uniqcmd()
      : proclimcmd(
            "-u", 0,
            "Read a set of move sequences on standard input and only echo\n"
            "those that are unique.  If an integer is attacheck to the -u "
            "option,\n"
            "exit after that many unique sequences have been seen.") {}
  virtual void docommand(puzdef &pd) { processlines(pd, uniqit); };
} registeruniq;
void wrongit(const puzdef &pd, setval p, const char *s) {
  int t = pd.numwrong(p, pd.solved);
  if (t <= maxwrong) {
    cout << t << " " << s << endl << flush;
    proclim--;
    if (proclim == 0)
      exit(0);
  }
}
static struct wrongcmd : cmd {
  wrongcmd()
      : cmd(0, "--maxwrong",
            "Takes an integer argument giving the a limit on the number of "
            "wrong\n"
            "pieces.  Read a set of move sequences on standard input and for "
            "each,\n"
            "if the number of wrong pieces is less than or equal to the "
            "integer\n"
            "given, echo the number of wrong pieces and the input sequence.") {}
  virtual void docommand(puzdef &pd) { processlines(pd, wrongit); };
  virtual void parse_args(int *argc, const char ***argv) {
    (*argc)++;
    (*argv)++;
    maxwrong = atoll(**argv);
  }
} registerwrong;
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
static struct symmuniqcmd : proclimcmd {
  symmuniqcmd()
      : proclimcmd(
            "-U", 0,
            "Read a set of move sequences on standard input and only echo\n"
            "those that are unique with respect to symmetry.  If an integer "
            "is\n"
            "attached to the -U option, exit after that many unique sequences "
            "have\n"
            "been seen.") {}
  virtual void docommand(puzdef &pd) { processlines(pd, uniqitsymm); };
} registersymmuniq;
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
static struct invertcmd : cmd {
  invertcmd()
      : cmd("-i", 0,
            "Read a set of move sequences on standard input and echo the\n"
            "inverted sequences.") {}
  virtual void docommand(puzdef &pd) { processlines4(pd, invertit); };
} registerinvert;
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
static struct cancelcmd : cmd {
  cancelcmd()
      : cmd(0, "--cancelseqs",
            "Read a set of move sequences on standard input and merge any\n"
            "nearly adjacent moves according to canonical sequences.  This "
            "does not\n"
            "reorder moves so the result is canonical; it just cancels "
            "moves.") {}
  virtual void docommand(puzdef &pd) { processlines3(pd, cancelit); };
} registercancel;
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
static struct mergecmd : cmd {
  mergecmd()
      : cmd(0, "--mergeseqs",
            "Read a set of move sequences on standard input and merge any\n"
            "nearly adjacent moves according to canonical sequences.  This "
            "also\n"
            "reorders moves so the end result is a canonical sequence.") {}
  virtual void docommand(puzdef &pd) { processlines3(pd, mergeit); };
} registermerge;
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
static struct shortencmd : cmd {
  shortencmd()
      : cmd(0, "--shortenseqs",
            "Read a set of move sequences on standard input and attempt\n"
            "to shorten each by optimally solving increasingly longer "
            "subsequences.") {}
  virtual void docommand(puzdef &pd) { processlines3(pd, shortenit); };
} registershorten;
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
static struct unrotatecmd : cmd {
  unrotatecmd()
      : cmd(0, "--unrotateseqs",
            "Read a set of move sequences on standard input and attempt\n"
            "to move all rotations to the end of the sequence.") {}
  virtual void docommand(puzdef &pd) { processlines4(pd, unrotateit); };
} registerunrotate;
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
static struct showrandompositioncmd : llcmd {
  showrandompositioncmd()
      : llcmd("-r", 0, "Show a given count of random positions", &rcnt) {}
  ll rcnt;
  virtual void docommand(puzdef &pd) {
    for (ll i = 0; i < rcnt; i++)
      showrandompos(pd);
  }
} registerrandpositioncmd;
// basic infrastructure for walking a set of sequences
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
