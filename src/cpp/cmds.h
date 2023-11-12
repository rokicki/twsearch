#ifndef CMDS_H
#include "util.h"
extern struct cmd *cmdhead;
struct cmd {
  cmd(const char *shorto, const char *longo, const char *docs)
      : shortoption(shorto), longoption(longo), userdocs(docs) {
    next = cmdhead;
    cmdhead = this;
  }
  const char *shortoption, *longoption;
  const char *userdocs;
  virtual void parse_args(int *argc, const char ***argv) = 0;
  virtual void docommand(puzdef &pd) = 0;
  virtual int ismaincmd() { return 1; }
  cmd *next;
};
struct boolopt : cmd {
  boolopt(const char *shorto, const char *longo, const char *docs, int *v)
      : cmd(shorto, longo, docs), var(v) {}
  virtual void parse_args(int *, const char ***) { *var = 1; }
  virtual void docommand(puzdef &) { error("! bad docommand"); }
  virtual int ismaincmd() { return 0; }
  int *var;
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
#define CMDS_H
#endif
