#include "descsets.h"
#include <iostream>
void descsets(puzdef &pd) {
  for (int i = 0; i < (int)pd.setdefs.size(); i++) {
    const setdef &sd = pd.setdefs[i];
    int n = sd.size;
    for (int j = 0; j < n; j++) {
      cout << sd.name << " " << (1 + j);
      for (int k = 0; k < (int)pd.basemoves.size(); k++) {
        if (pd.basemoves[k].pos.dat[sd.off + j] != j ||
            pd.basemoves[k].pos.dat[sd.off + j + n] != 0)
          cout << " " << pd.basemoves[k].name;
      }
      cout << endl;
    }
  }
}
