#include "cmds.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <string>
cmd *cmdhead;
void printhelp() {
  map<string, cmd *> sortme;
  for (auto p = cmdhead; p; p = p->next) {
    const char *q = p->option;
    while (*q == '-')
      q++;
    string k;
    for (const char *t = q; *t; t++)
      if ('A' <= *t && *t <= 'Z')
        k.push_back((char)(*t + 32)); // uppercase
      else
        k.push_back(*t);
    k += " ";
    k += q;
    if (sortme.find(k) != sortme.end())
      error("! duplicated option");
    sortme[k] = p;
  }
  for (auto it : sortme) {
    auto p = it.second;
    cout << p->option << " ";
    cout << " ";
    for (const char *c = p->userdocs; *c; c++)
      if (*c == '\n')
        cout << endl << "   ";
      else
        cout << *c;
    cout << endl << endl;
  }
}
