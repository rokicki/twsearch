#ifndef INDEX_H
#include "city.h"
#include "puzdef.h"
/*
 *   These are all the routines that convert a puzzle state or position
 *   into a compact encoding.  Some of these are dense and others are
 *   less dense but faster.
 */
extern vector<pair<ull, int>> parts;
extern int looseper, looseiper, basebits, usehashenc;
void calclooseper(const puzdef &pd);
ull permtoindex(const uchar *perm, int n);
void indextoperm(uchar *perm, ull ind, int n);
ull permtoindex2(const uchar *perm, int n);
void indextoperm2(uchar *perm, ull ind, int n);
ull mpermtoindex(const uchar *perm, int n);
void indextomperm(uchar *perm, ull ind, const vector<int> &cnts);
ll ordstoindex(const uchar *p, int omod, int n);
void indextoords(uchar *p, ull v, int omod, int n);
void indextoords2(uchar *p, ull v, int omod, int n);
ull densepack(const puzdef &pd, setval pos);
void denseunpack(const puzdef &pd, ull v, setval pos);
ull densepack_ordered(const puzdef &pd, setval pos);
ull denseunpack_ordered(const puzdef &pd, ull v, setval pos);
void loosepack(const puzdef &pd, setval pos, loosetype *w, int fromid = 0,
               int sym = 0);
void looseunpack(const puzdef &pd, setval pos, loosetype *r);
int looseperone(const puzdef &pd, int sdi, int symm);
void loosepackone(const puzdef &pd, setval pos, int sdi, loosetype *w,
                  int fromid = 0, int sym = 0);
void looseunpackone(const puzdef &pd, setval pos, int sdi, loosetype *r);
/*
 *   Some stuff to allow us to use positions in STL containers.
 */
template <typename T> struct hashvector {
  size_t operator()(const vector<T> &v) const {
    return CityHash64((const char *)v.data(), sizeof(T) * v.size());
  }
};
template <typename T> void freeContainer(T &c) {
  T empty;
  swap(c, empty);
}
#define INDEX_H
#endif
