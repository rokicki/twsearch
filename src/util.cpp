#include "util.h"
#include <random>
#include <iostream>
#include <cstdlib>
#include <sys/time.h>
double start ;
int verbose = 1 ;
ll maxmem = 8LL * 1024LL * 1024LL * 1024LL ;
int quarter ;
double walltime() {
   struct timeval tv ;
   gettimeofday(&tv, 0) ;
   return tv.tv_sec + 0.000001 * tv.tv_usec ;
}
double duration() {
   double now = walltime() ;
   double r = now - start ;
   start = now ;
   return r ;
}
string curline ;
void error(string msg, string extra) {
   cerr << msg << extra << endl ;
   if (curline.size() > 0)
      cerr << "At: " << curline << endl ;
   exit(10) ;
}
void warn(string msg, string extra) {
   cerr << msg << extra << endl ;
}
/*
 *   strdup is going through some issues: POSIX vs C++, so we just
 *   implement it ourselves.
 */
const char *twstrdup(const char *s) {
   char *r = (char *)malloc(strlen(s)+1) ;
   strcpy(r, s) ;
   return r ;
}
double myrand(int n) {
   static mt19937 rng ;
   // the following double is exact
   static double mul = 1.0 / (rng.max() - rng.min() + 1.0) ;
   return (int)((rng()-rng.min()) * mul * n) ;
}
ll gcd(ll a, ll b) {
   if (a > b)
      swap(a, b) ;
   if (a == 0)
      return b ;
   return gcd(b % a, a) ;
}
ll lcm(ll a, ll b) {
   return a / gcd(a,b) * b ;
}
int ceillog2(int v) {
   int r = 0 ;
   while (v > (1 << r))
      r++ ;
   return r ;
}
void init_util() {
   duration() ;
}
int isprime(int p) {
   if (p < 2)
      return 0 ;
   if (p < 4)
      return 1 ;
   if ((p & 1) == 0)
      return 0 ;
   for (int j=3; ; j+=2) {
      if (p % j == 0)
         return 0 ;
      if (j * j > p)
         return 1 ;
   }
}
