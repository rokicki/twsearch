#ifndef INDEX_H
#include "puzdef.h"
extern vector<pair<ull, int> > parts ;
extern int looseper, looseiper ;
void calclooseper(const puzdef &pd) ;
long long permtoindex(const uchar *perm, int n) ;
void indextoperm(uchar *perm, ull ind, int n) ;
ull permtoindex2(const uchar *perm, int n) ;
void indextoperm2(uchar *perm, ull ind, int n) ;
ll ordstoindex(const uchar *p, int omod, int n) ;
void indextoords(uchar *p, ull v, int omod, int n) ;
void indextoords2(uchar *p, ull v, int omod, int n) ;
ull densepack(const puzdef &pd, setval pos) ;
void denseunpack(const puzdef &pd, ull v, setval pos) ;
ull densepack_ordered(const puzdef &pd, setval pos) ;
ull denseunpack_ordered(const puzdef &pd, ull v, setval pos) ;
void loosepack(const puzdef &pd, setval pos, loosetype *w, int fromid=0) ;
void looseunpack(const puzdef &pd, setval pos, loosetype *r) ;
#define INDEX_H
#endif
