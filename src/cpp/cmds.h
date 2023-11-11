#ifndef CMDS_H
extern struct cmd *cmdhead;
struct cmd {
  cmd(const char *shorto, const char *longo, const char *docs)
      : shortoption(shorto), longoption(longo), userdocs(docs) {
    next = cmdhead;
    cmdhead = this;
  }
  const char *shortoption, *longoption;
  const char *userdocs;
  virtual void parse_args(int *argc, const char ***argv);
  virtual void docommand(puzdef &pd);
  virtual int ismaincmd() { return 1; }
  cmd *next;
};
#define CMDS_H
#endif
