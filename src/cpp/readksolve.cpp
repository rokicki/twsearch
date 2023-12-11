#include "readksolve.h"
#include "parsemoves.h"
#include <iostream>
int nocorners, nocenters, noedges, ignoreori, distinguishall;
set<string> omitsets;
static int lineno;
void inerror(const string s, const string x = "") {
  if (lineno)
    cerr << lineno << ": ";
  error(s, x);
}
vector<string> getline(istream *f, ull &checksum) {
  string s;
  int c;
  lineno++;
  while (1) {
    s.clear();
    while (1) {
      c = f->get();
      if (c != EOF)
        checksum = 31 * checksum + c;
      if (c == EOF || c == 10 || c == 13) {
        if (c == EOF || s.size() > 0)
          break;
        else {
          if (c == 10)
            lineno++;
          continue;
        }
      }
      s.push_back((char)c);
    }
    vector<string> toks;
    if (s.size() == 0) {
      curline = s;
      return toks;
    }
    if (verbose > 2)
      cout << ">> " << s << endl;
    if (s[0] == '#') {
      lineno++;
      continue;
    }
    string tok;
    for (int i = 0; i < (int)s.size(); i++) {
      if (s[i] <= ' ') {
        if (tok.size() > 0) {
          toks.push_back(tok);
          tok.clear();
        }
      } else {
        tok.push_back(s[i]);
      }
    }
    if (tok.size() > 0)
      toks.push_back(tok);
    if (toks.size() == 0) {
      lineno++;
      continue;
    }
    curline = s;
    return toks;
  }
}
void expect(const vector<string> &toks, int cnt) {
  if (cnt != (int)toks.size())
    inerror("! wrong number of tokens on line");
}
// must be a number under 256.
int getnumber(int minval, const string &s) {
  int r = 0;
  for (int i = 0; i < (int)s.size(); i++) {
    if ('0' <= s[i] && s[i] <= '9')
      r = r * 10 + s[i] - '0';
    else
      inerror("! bad character while parsing number in ", s);
  }
  if (r < minval || r > 255)
    inerror("! value out of range in ", s);
  return r;
}
// permits a ? for undefined.
int getnumberorneg(int minval, const string &s) {
  if (s == "?")
    return 255;
  int r = 0;
  for (int i = 0; i < (int)s.size(); i++) {
    if ('0' <= s[i] && s[i] <= '9')
      r = r * 10 + s[i] - '0';
    else
      inerror("! bad character while parsing number in ", s);
  }
  if (r < minval || r > 255)
    inerror("! value out of range in ", s);
  return r;
}
int isnumber(const string &s) {
  return s.size() > 0 && '0' <= s[0] && s[0] <= '9';
}
int oddperm(uchar *p, int n) {
  static uchar done[256];
  for (int i = 0; i < n; i++)
    done[i] = 0;
  int r = 0;
  for (int i = 0; i < n; i++)
    if (!done[i]) {
      int cnt = 1;
      done[i] = 1;
      for (int j = p[i]; !done[j]; j = p[j]) {
        done[j] = 1;
        cnt++;
      }
      if (0 == (cnt & 1))
        r++;
    }
  return r & 1;
}
int omitset(string s) {
  if (omitsets.find(s) != omitsets.end())
    return 1;
  if (s.size() < 2)
    return 0;
  if (nocorners && tolower(s[0]) == 'c' && s[1] != 0 && tolower(s[2]) == 'r')
    return 1;
  if (nocenters && tolower(s[0]) == 'c' && tolower(s[1]) == 'e')
    return 1;
  if (noedges && tolower(s[0]) == 'e' && tolower(s[1]) == 'd')
    return 1;
  return 0;
}
allocsetval readposition(puzdef &pz, char typ, istream *f, ull &checksum,
                         bool zero_indexed) {
  allocsetval r(pz, pz.id);
  int curset = -1;
  int numseq = 0;
  int ignore = 0;
  while (1) {
    vector<string> toks = getline(f, checksum);
    if (toks.size() == 0)
      inerror("! premature end while reading position");
    if (toks[0] == "End") {
      if (curset >= 0 && numseq == 0 && ignore == 0)
        inerror("! empty set def?");
      expect(toks, 1);
      ignore = 0;
      break;
    }
    if ((typ != 'm' && toks[0] == "?") || isnumber(toks[0])) {
      if (ignore)
        continue;
      if (curset < 0 || numseq > 1)
        inerror("! unexpected number sequence");
      int n = pz.setdefs[curset].size;
      expect(toks, n);
      uchar *p = r.dat + pz.setdefs[curset].off + numseq * n;
      int offset = (numseq == 0 && zero_indexed) ? 0 : 1;
      if (typ != 'm') {
        for (int i = 0; i < n; i++) {
          p[i] = getnumberorneg(offset - numseq, toks[i]);
          if (p[i] != 255) {
            p[i] += 1 - offset;
          }
        }
      } else {
        for (int i = 0; i < n; i++) {
          p[i] = getnumber(offset - numseq, toks[i]);
          if (p[i] != 255) {
            p[i] += 1 - offset;
          }
        }
      }
      if (numseq == 1 && ignoreori)
        for (int i = 0; i < n; i++)
          p[i] = 0;
      numseq++;
    } else {
      if (curset >= 0 && numseq == 0)
        inerror("! empty set def?");
      expect(toks, 1);
      ignore = 0;
      if (omitset(toks[0])) {
        ignore = 1;
        continue;
      }
      curset = -1;
      for (int i = 0; i < (int)pz.setdefs.size(); i++)
        if (toks[0] == pz.setdefs[i].name) {
          curset = i;
          break;
        }
      if (curset < 0)
        inerror("Bad set name?");
      if (r.dat[pz.setdefs[curset].off])
        inerror("! redefined set name?");
      numseq = 0;
    }
  }
  for (int i = 0; i < (int)pz.setdefs.size(); i++) {
    uchar *p = r.dat + pz.setdefs[i].off;
    int n = pz.setdefs[i].size;
    vector<int> cnts;
    if (p[0] == 0 || (typ == 's' && distinguishall)) {
      if (typ == 'S') {
        for (int j = 0; j < n; j++)
          p[j] = pz.solved.dat[pz.setdefs[i].off + j];
      } else {
        cnts.resize(n);
        for (int j = 0; j < n; j++) {
          p[j] = j; // identity perm
          cnts[j]++;
        }
        if (typ == 's') {
          pz.setdefs[i].psum = n * (n - 1) / 2;
          pz.setdefs[i].cnts = cnts;
        }
      }
    } else {
      int sum = 0;
      for (int j = 0; j < n; j++) {
        int v = --p[j];
        sum += v;
        if (v >= (int)cnts.size())
          cnts.resize(v + 1);
        cnts[v]++;
      }
      if (typ == 's')
        pz.setdefs[i].psum = sum;
      for (int j = 0; j < (int)cnts.size(); j++)
        if (cnts[j] == 0)
          inerror("! values are not contiguous");
      if (typ == 'S' && !(cnts == pz.setdefs[i].cnts))
        inerror("! scramble position permutation doesn't match solved");
      if (typ == 's')
        pz.setdefs[i].cnts = cnts;
      if ((int)cnts.size() != n) {
        if (typ != 's' && typ != 'S')
          inerror("! expected, but did not see, a proper permutation");
        else {
          pz.setdefs[i].uniq = 0;
          pz.uniq = 0;
          pz.setdefs[i].pbits = ceillog2(cnts.size());
          if (n > 64) {
            pz.setdefs[i].dense = 0;
            pz.dense = 0;
          }
        }
      } else {
        if (typ != 'S' && oddperm(p, n))
          pz.setdefs[i].pparity = 0;
      }
    }
    p += n;
    int s = 0;
    int checkwildo = 0;
    for (int j = 0; j < n; j++) {
      if (p[j] == 255 && typ != 'm') {
        if (pz.setdefs[i].omod == 1) {
          p[j] = 0;
        } else {
          p[j] = 2 * pz.setdefs[i].omod;
          if (typ == 's') {
            pz.setdefs[i].wildo = 1;
            pz.setdefs[i].obits = ceillog2(pz.setdefs[i].omod + 1);
            pz.wildo = 1;
            pz.setdefs[i].oparity = 0;
          } else {
            if (typ != 'S')
              inerror("! internal error; should be reading scramble");
            checkwildo = 1;
          }
        }
      } else if (p[j] >= pz.setdefs[i].omod) {
        inerror("! modulo value too large");
      }
      s += p[j];
    }
    // ensure all identical pieces either are not wild orientation
    // or are all wild orientation
    if (typ == 's' && pz.setdefs[i].wildo) {
      for (int j = 0; j < n; j++)
        for (int k = j + 1; k < n; k++)
          if (p[j - n] == p[k - n])
            if ((p[j] < pz.setdefs[i].omod) != (p[k] < pz.setdefs[i].omod))
              inerror("! inconsistent orientation wildcards across identical "
                      "pieces");
    }
    if (typ == 'S' && (checkwildo || pz.setdefs[i].wildo)) {
      if (!checkwildo)
        inerror(
            "! solved state in def has ? orientation but scramble does not");
      if (!pz.setdefs[i].wildo)
        inerror("! scramble def has ? orientation but solved state in def does "
                "not");
      for (int j = 0; j < n; j++)
        for (int k = 0; k < n; k++)
          if (p[j - n] == pz.solved.dat[pz.setdefs[i].off + k])
            if ((p[j] < pz.setdefs[i].omod) !=
                (pz.solved.dat[pz.setdefs[i].off + n + k] < pz.setdefs[i].omod))
              inerror("! inconsistent use of orientation wildcards between "
                      "solved state and scramble");
    }
    if (s % pz.setdefs[i].omod != 0)
      pz.setdefs[i].oparity = 0;
    if (typ == 'm' && !zero_indexed) { // fix moves
      static uchar f[256];
      for (int j = 0; j < n; j++)
        f[j] = p[j];
      for (int j = 0; j < n; j++)
        p[j] = f[p[j - n]];
    }
  }
  return r;
}
puzdef readdef(istream *f) {
  curline.clear();
  puzdef pz;
  int state = 0;
  ull checksum = 0;
  pz.optionssum = 0;
  lineno = 0;
  while (1) {
    vector<string> toks = getline(f, checksum);
    if (toks.size() == 0)
      break;
    if (toks[0] == "Name") {
      if (state != 0)
        inerror("! Name in wrong place");
      state++;
      expect(toks, 2);
      pz.name = toks[1];
    } else if (toks[0] == "Set") {
      if (state == 0) {
        pz.name = "Unnamed";
        state++;
      }
      if (state != 1)
        inerror("! Set in wrong place");
      expect(toks, 4);
      if (omitset(toks[1]))
        continue;
      setdef sd;
      sd.name = toks[1];
      sd.size = getnumber(1, toks[2]);
      sd.omod = getnumber(1, toks[3]);
      if (sd.omod > 127)
        inerror("! twsearch supports a maximum orientation size of 127");
      if (ignoreori)
        sd.omod = 1;
      sd.pparity = (sd.size == 1 ? 0 : 1);
      sd.oparity = 1;
      sd.pbits = ceillog2(sd.size);
      sd.pibits = sd.pbits;
      if (sd.wildo) {
        sd.obits = ceillog2(sd.omod + 1);
      } else {
        sd.obits = ceillog2(sd.omod);
      }
      sd.uniq = 1;
      sd.dense = 1;
      sd.off = pz.totsize;
      pz.setdefs.push_back(sd);
      pz.totsize += 2 * sd.size;
      if (gmoda[sd.omod] == 0) {
        gmoda[sd.omod] = (uchar *)calloc(4 * sd.omod, 1);
        for (int i = 0; i < 2 * sd.omod; i++)
          gmoda[sd.omod][i] = i % sd.omod;
        for (int i = 2 * sd.omod; i < 4 * sd.omod; i++)
          gmoda[sd.omod][i] = 2 * sd.omod;
      }
    } else if (toks[0] == "Illegal") {
      if (state < 2)
        inerror("! Illegal must be after solved");
      // set name, position, value, value, value, value ...
      for (int i = 3; i < (int)toks.size(); i++)
        pz.addillegal(toks[1].c_str(), getnumber(1, toks[2]),
                      getnumber(1, toks[i]));
    } else if (toks[0] == "Solved" || toks[0] == "StartState") {
      if (state != 1)
        inerror("! Solved in wrong place");
      state++;
      expect(toks, 1);
      pz.id = allocsetval(pz, -1);
      uchar *p = pz.id.dat;
      for (int i = 0; i < (int)pz.setdefs.size(); i++) {
        int n = pz.setdefs[i].size;
        for (int j = 0; j < n; j++)
          p[j] = j;
        p += n;
        for (int j = 0; j < n; j++)
          p[j] = 0;
        p += n;
      }
      pz.solved = readposition(pz, 's', f, checksum, toks[0] == "StartState");
    } else if (toks[0] == "Move" || toks[0] == "MoveTransformation") {
      if (state != 2)
        inerror("! Move in wrong place");
      expect(toks, 2);
      moove m(pz, pz.id);
      m.name = toks[1];
      m.pos =
          readposition(pz, 'm', f, checksum, toks[0] == "MoveTransformation");
      m.cost = 1;
      m.twist = 1;
      if (isrotation(m.name)) {
        m.base = pz.rotations.size();
        pz.rotations.push_back(m);
      } else {
        m.base = pz.moves.size();
        pz.moves.push_back(m);
      }
    } else if (toks[0] == "MoveAlias") {
      if (state != 2)
        inerror("! MoveAlias in wrong place");
      expect(toks, 3);
      pz.aliases.push_back({toks[1], toks[2]});
    } else if (toks[0] == "MoveSequence") {
      if (state != 2)
        inerror("! MoveSequence in wrong place");
      if (toks.size() < 3)
        inerror("Too few tokens in MoveSequence definition");
      string seq;
      for (int i = 2; i < (int)toks.size(); i++) {
        if (i != 2)
          seq += " ";
        seq += toks[i];
      }
      pz.moveseqs.push_back({toks[1], seq});
    } else {
      inerror("! unexpected first token on line ", toks[0]);
    }
  }
  if (pz.name.size() == 0)
    inerror("! puzzle must be given a name");
  if (pz.setdefs.size() == 0)
    inerror("! puzzle must have set definitions");
  if (pz.solved.dat == 0)
    inerror("! puzzle must have a solved position");
  if (pz.moves.size() == 0)
    inerror("! puzzle must have moves");
  lineno = 0;
  if (distinguishall) {
    pz.solved = pz.id;
  }
  pz.caninvert = pz.uniq && !pz.wildo;
  pz.checksum = checksum;
  curline.clear();
  return pz;
}
void expandmoveset(const puzdef &pd, vector<moove> &moves,
                   vector<moove> &newmoves, vector<string> &newnames,
                   vector<int> &basemoveorders) {
  stacksetval p1(pd), p2(pd);
  for (int i = 0; i < (int)moves.size(); i++) {
    moove &m = moves[i];
    if (quarter && m.cost > 1)
      continue;
    vector<allocsetval> movepowers;
    movepowers.push_back(m.pos);
    pd.assignpos(p1, m.pos);
    pd.assignpos(p2, m.pos);
    for (int p = 2; p < 256; p++) {
      pd.mul(p1, m.pos, p2);
      if (pd.comparepos(p2, pd.id) == 0)
        break;
      movepowers.push_back(allocsetval(pd, p2));
      swap(p1.dat, p2.dat);
    }
    int order = movepowers.size() + 1;
    basemoveorders.push_back(order);
    for (int j = 0; j < (int)movepowers.size(); j++) {
      int tw = j + 1;
      if (order - tw < tw)
        tw -= order;
      moove m2 = m;
      m2.pos = movepowers[j];
      m2.cost = abs(tw);
      m2.twist = (tw + order) % order;
      if (tw != 1) {
        string s2 = m.name;
        if (tw != -1)
          s2 += to_string(abs(tw));
        if (tw < 0)
          s2 += "'";
        newnames.push_back(s2);
        m2.name = s2;
      }
      newmoves.push_back(m2);
    }
  }
}
void addmovepowers(puzdef &pd) {
  vector<moove> newmoves;
  pd.basemoves = pd.moves;
  vector<string> newnames;
  expandmoveset(pd, pd.rotations, newmoves, newnames, pd.baserotorders);
  if (newnames.size() > 0) {
    pd.expandedrotations = newmoves;
    if (verbose) {
      cout << "Created new rotations";
      for (int i = 0; i < (int)newnames.size(); i++)
        cout << " " << newnames[i];
      cout << endl << flush;
    }
  } else {
    pd.expandedrotations = pd.rotations;
  }
  newmoves.clear();
  newnames.clear();
  expandmoveset(pd, pd.moves, newmoves, newnames, pd.basemoveorders);
  if (newnames.size() > 0) {
    pd.moves = newmoves;
    if (verbose) {
      cout << "Created new moves";
      for (int i = 0; i < (int)newnames.size(); i++)
        cout << " " << newnames[i];
      cout << endl << flush;
    }
  } else {
    pd.moves = pd.basemoves;
  }
}
