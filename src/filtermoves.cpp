#include <map>
#include "filtermoves.h"
#include "parsemoves.h"
/*
 *   Rewrite the movelist in the puzzle definition to restrict moves.
 *   This is a bit tricky.  The moves in the move list can be base
 *   moves (like U) or derived moves (like U2 or U').  In all cases
 *   we include only appropriate multiples.
 */
int goodmove(const moove &mv, int inc, int order) {
   if (inc == 0)
      return 0 ;
   if (order % inc != 0)
      error("! filtered move has to be simplest possible") ;
   // there's a faster number theory way to do this, but why.
   return (mv.twist % inc == 0) ;
}
void filtermovelist(puzdef &pd, const char *movelist) {
   vector<int> moves = parsemovelist(pd, movelist) ;
   vector<int> lowinc(pd.basemoves.size()) ;
   for (int i=0; i<(int)moves.size(); i++) {
      moove &mv = pd.moves[moves[i]] ;
      if (lowinc[mv.base])
         error("Move list restriction should only list a base move once.") ;
      lowinc[mv.base] = mv.twist ;
   }
   vector<moove> newbase ;
   map<int, int> moveremap ;
   vector<int> newbasemoveorders ;
   for (int i=0; i<(int)pd.basemoves.size(); i++)
      if (goodmove(pd.basemoves[i], lowinc[i], pd.basemoveorders[i])) {
         int newbasenum = newbase.size() ;
         moove newmv = pd.basemoves[i] ;
         newmv.base = newbasenum ;
         moveremap[i] = newbasenum ;
         newbase.push_back(newmv) ;
         newbasemoveorders.push_back(pd.basemoveorders[i] / lowinc[i]) ;
      } else {
      }
   vector<moove> newmvs ;
   for (int i=0; i<(int)pd.moves.size(); i++) {
      int obase = pd.moves[i].base ;
      if (goodmove(pd.moves[i], lowinc[obase], pd.basemoveorders[obase])) {
         moove newmv = pd.moves[i] ;
         int otwist = newmv.twist ;
         newmv.twist /= lowinc[pd.moves[i].base] ;
         if (otwist == lowinc[obase] && lowinc[obase] > 1) {
            int newbasenum = newbase.size() ;
            moveremap[obase] = newbasenum ;
            newmv.base = newbasenum ;
            newmv.cost /= lowinc[obase] ;
            newbase.push_back(newmv) ;
            newbasemoveorders.push_back(pd.basemoveorders[obase] / lowinc[obase]) ;
         }
         newmv.base = moveremap[obase] ;
         newmvs.push_back(newmv) ;
      }
   }
   // allow parsing to pick up old move positions
   pd.parsemoves = pd.moves ;
   pd.basemoveorders = newbasemoveorders ;
   pd.basemoves = newbase ;
   pd.moves = newmvs ;
   pd.addoptionssum(movelist) ;
}
