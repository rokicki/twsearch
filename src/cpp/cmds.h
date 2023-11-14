#ifndef CMDS_H
#include "util.h"
#include <inttypes.h>
#include <limits.h>
extern struct cmd *cmdhead;
struct puzdef;
struct cmd {
  cmd(const char *shorto, const char *longo, const char *docs)
      : shortoption(shorto), longoption(longo), userdocs(docs) {
    next = cmdhead;
    cmdhead = this;
  }
  const char *shortoption, *longoption;
  const char *userdocs;
  virtual void parse_args(int *, const char ***) {}
  virtual void docommand(puzdef &pd) = 0;
  virtual int ismaincmd() { return 1; }
  cmd *next;
};
struct specialopt : cmd {
  specialopt(const char *shorto, const char *longo, const char *docs)
      : cmd(shorto, longo, docs) {}
  virtual void parse_args(int *, const char ***) = 0;
  virtual void docommand(puzdef &) { error("! bad docommand"); }
  virtual int ismaincmd() { return 0; }
};
struct boolopt : cmd {
  boolopt(const char *shorto, const char *longo, const char *docs, int *v)
      : cmd(shorto, longo, docs), var(v) {}
  virtual void parse_args(int *, const char ***) { *var = 1; }
  virtual void docommand(puzdef &) { error("! bad docommand"); }
  virtual int ismaincmd() { return 0; }
  int *var;
};
struct intopt : cmd {
  intopt(const char *shorto, const char *longo, const char *docs, int *v,
         int lo, int hi)
      : cmd(shorto, longo, docs), var(v), lolim(lo), hilim(hi) {}
  virtual void parse_args(int *argc, const char ***argv) {
    (*argc)--;
    (*argv)++;
    char *endptr = 0;
    *var = strtoimax(**argv, &endptr, 10);
    if (***argv == 0 || *endptr != 0)
      error("! bad integer argument");
    if (*var < lolim || *var > hilim)
      error("! argument out of range");
  }
  virtual void docommand(puzdef &) { error("! bad docommand"); }
  virtual int ismaincmd() { return 0; }
  int *var, lolim, hilim;
};
struct llopt : cmd {
  llopt(const char *shorto, const char *longo, const char *docs, ll *v)
      : cmd(shorto, longo, docs), var(v) {}
  virtual void parse_args(int *argc, const char ***argv) {
    (*argc)--;
    (*argv)++;
    char *endptr = 0;
    *var = strtoll(**argv, &endptr, 10);
    if (***argv == 0 || *endptr != 0)
      error("! bad integer argument");
  }
  virtual void docommand(puzdef &) { error("! bad docommand"); }
  virtual int ismaincmd() { return 0; }
  ll *var;
};
struct stringopt : cmd {
  stringopt(const char *shorto, const char *longo, const char *docs,
            const char **v)
      : cmd(shorto, longo, docs), var(v) {}
  virtual void parse_args(int *argc, const char ***argv) {
    (*argc)--;
    (*argv)++;
    *var = **argv;
  }
  virtual void docommand(puzdef &) { error("! bad docommand"); }
  virtual int ismaincmd() { return 0; }
  const char **var;
};
struct llcmd : llopt {
  llcmd(const char *shorto, const char *longo, const char *docs, ll *v)
      : llopt(shorto, longo, docs, v) {}
  virtual int ismaincmd() { return 1; }
};
void printhelp();
#define CMDS_H
#endif
