#include "filtermoves.h"
#include "parsemoves.h"
#include <map>
/*
 *   Rewrite the movelist in the puzzle definition to restrict moves.
 *   This is a bit tricky.  The moves in the move list can be base
 *   moves (like U) or derived moves (like U2 or U').  In all cases
 *   we include only appropriate multiples.
 */
int goodmove(const moove &mv, int inc, int order) {
  if (inc == 0)
    return 0;
  if (order % inc != 0)
    error("! filtered move has to be simplest possible");
  // there's a faster number theory way to do this, but why.
  return (mv.twist % inc == 0);
}
void filtermovelist(puzdef &pd, const char *movelist) {
  int nummoves = pd.moves.size();
  int numbmoves = pd.basemoves.size();
  vector<int> moves = parsemoveorrotationlist(pd, movelist);
  vector<int> lowinc(pd.basemoves.size() + pd.baserotations.size());
  for (int i = 0; i < (int)moves.size(); i++) {
    moove &mv = moves[i] >= nummoves ? pd.expandedrotations[moves[i] - nummoves]
                                     : pd.moves[moves[i]];
    int obase = moves[i] >= nummoves ? numbmoves + mv.base : mv.base;
    if (lowinc[obase])
      error("Move list restriction should only list a base move once.");
    lowinc[obase] = mv.twist;
  }
  vector<moove> newbase;
  map<int, int> moveremap;
  vector<int> newbasemoveorders;
  for (int i = 0; i < (int)pd.basemoves.size() + (int)pd.baserotorders.size();
       i++) {
    moove &bm =
        i >= numbmoves ? pd.baserotations[i - numbmoves] : pd.basemoves[i];
    int bmi =
        i >= numbmoves ? pd.baserotorders[i - numbmoves] : pd.basemoveorders[i];
    if (goodmove(bm, lowinc[i], bmi)) {
      int newbasenum = newbase.size();
      moove newmv = bm;
      newmv.base = newbasenum;
      moveremap[i] = newbasenum;
      newbase.push_back(newmv);
      newbasemoveorders.push_back(bmi / lowinc[i]);
    }
  }
  vector<moove> newmvs;
  for (int i = 0; i < (int)pd.moves.size() + (int)pd.expandedrotations.size();
       i++) {
    moove &bm =
        i >= nummoves ? pd.expandedrotations[i - nummoves] : pd.moves[i];
    int obase = i >= nummoves ? numbmoves + bm.base : bm.base;
    int bmi = obase >= numbmoves ? pd.baserotorders[obase - numbmoves]
                                 : pd.basemoveorders[obase];
    if (goodmove(bm, lowinc[obase], bmi)) {
      moove newmv = bm;
      int otwist = newmv.twist;
      newmv.twist /= lowinc[obase];
      if (otwist == lowinc[obase] && lowinc[obase] > 1) {
        int newbasenum = newbase.size();
        moveremap[obase] = newbasenum;
        newmv.base = newbasenum;
        newmv.cost /= lowinc[obase];
        newbase.push_back(newmv);
        newbasemoveorders.push_back(bmi / lowinc[obase]);
      }
      newmv.base = moveremap[obase];
      newmvs.push_back(newmv);
    }
  }
  // allow parsing to pick up old move positions
  pd.parsemoves = pd.moves;
  pd.basemoveorders = newbasemoveorders;
  pd.basemoves = newbase;
  pd.moves = newmvs;
  pd.addoptionssum(movelist);
}
