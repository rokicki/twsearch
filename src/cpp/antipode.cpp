#include "antipode.h"
#include "cmds.h"
#include "index.h"
#include <iostream>
ll antipodecount = 20;
ll antipodeshave;
int resetonnewantipode = 0;
loosetype *antipodesloose;
ull *antipodesdense;
void emitposition(const puzdef &pd, setval p, const char *s);
void looseunpack(const puzdef &pd, setval pos, loosetype *r);
ull denseunpack_ordered(const puzdef &pd, ull v, setval pos);
void showantipodes(const puzdef &pd, loosetype *beg, loosetype *end) {
  ll t = (end - beg) / looseper;
  if (t > antipodecount)
    t = antipodecount;
  antipodeshave = t;
  cout << "Showing " << antipodeshave << " antipodes." << endl;
  stacksetval pos(pd);
  for (int i = 0; i < antipodeshave; i++) {
    looseunpack(pd, pos, beg + i * looseper);
    emitposition(pd, pos, nullptr);
  }
}
void resetantipodes() { resetonnewantipode = 1; }
void showantipodesloose(const puzdef &pd) {
  showantipodes(pd, antipodesloose, antipodesloose + looseper * antipodecount);
}
void showantipodesdense(const puzdef &pd, int ordered) {
  if (antipodesdense == 0)
    error("! no antipodes?");
  cout << "Showing " << antipodeshave << " antipodes." << endl;
  stacksetval pos(pd);
  for (int i = 0; i < antipodeshave; i++) {
    if (ordered)
      denseunpack_ordered(pd, antipodesdense[i], pos);
    else
      denseunpack(pd, antipodesdense[i], pos);
    emitposition(pd, pos, nullptr);
  }
}
void stashantipodesloose(loosetype *beg, loosetype *end) {
  if (end == beg)
    return;
  if (antipodesloose == 0)
    antipodesloose =
        (loosetype *)calloc(antipodecount, looseper * sizeof(loosetype));
  ll t = (end - beg) / looseper;
  if (t > antipodecount)
    t = antipodecount;
  antipodeshave = t;
  memcpy(antipodesloose, beg, antipodeshave * looseper * sizeof(loosetype));
}
void stashantipodedense(ull val) {
  if (resetonnewantipode) {
    antipodeshave = 0;
    resetonnewantipode = 0;
  }
  if (antipodeshave < antipodecount) {
    if (antipodesdense == 0)
      antipodesdense = (ull *)calloc(antipodecount, sizeof(ull));
    antipodesdense[antipodeshave++] = val;
  }
}
static struct antipodecountopt : llopt {
  antipodecountopt()
      : llopt("-a",
              "num  Set the number of antipodes to print.  The default is 20.",
              &antipodecount) {}
} registermea;
