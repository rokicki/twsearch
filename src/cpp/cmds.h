#ifndef CMDS_H
#include "util.h"
#include <inttypes.h>
#include <limits.h>
extern struct cmd *cmdhead;
struct puzdef;
struct cmd {
  cmd(const char *opt, const char *docs) : option(opt), userdocs(docs) {
    next = cmdhead;
    cmdhead = this;
  }
  const char *option;
  const char *userdocs;
  virtual void parse_args(int *, const char ***) {}
  virtual void docommand(puzdef &pd) = 0;
  virtual int ismaincmd() { return 1; }
  cmd *next;
};
struct specialopt : cmd {
  specialopt(const char *opt, const char *docs) : cmd(opt, docs) {}
  virtual void parse_args(int *, const char ***) = 0;
  virtual void docommand(puzdef &) { error("! bad docommand"); }
  virtual int ismaincmd() { return 0; }
};
struct boolopt : cmd {
  boolopt(const char *opt, const char *docs, int *v) : cmd(opt, docs), var(v) {}
  virtual void parse_args(int *, const char ***) { *var = 1; }
  virtual void docommand(puzdef &) { error("! bad docommand"); }
  virtual int ismaincmd() { return 0; }
  int *var;
};
struct intopt : cmd {
  intopt(const char *opt, const char *docs, int *v, int lo, int hi)
      : cmd(opt, docs), var(v), lolim(lo), hilim(hi) {}
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
  llopt(const char *opt, const char *docs, ll *v) : cmd(opt, docs), var(v) {}
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
  stringopt(const char *opt, const char *docs, const char **v)
      : cmd(opt, docs), var(v) {}
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
  llcmd(const char *opt, const char *docs, ll *v) : llopt(opt, docs, v) {}
  virtual int ismaincmd() { return 1; }
};
void printhelp();
#define CMDS_H
#endif
