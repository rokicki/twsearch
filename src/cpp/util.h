#ifndef UTIL_H
#include <string>
#include <cstring>
#include <cstdlib>   // for drand48 and friends
#ifndef _WIN64
#include <strings.h> // for ffsll
#endif
using namespace std ;
/*
 *   A bunch of silly utility routines to make our life easier, and a few
 *   (ughh) globals.
 */
typedef long long ll ;
typedef unsigned long long ull ;
typedef unsigned char uchar ;
typedef unsigned int loosetype ;
const int BITSPERLOOSE = 8*sizeof(loosetype) ;
double walltime() ;
double duration() ;
void error(string msg, string extra="") ;
void warn(string msg, string extra="") ;
const char *twstrdup(const char *s) ;
double myrand(int n) ;
ll gcd(ll a, ll b) ;
ll lcm(ll a, ll b) ;
int ceillog2(int v) ;
int isprime(int p) ;
void init_util() ;
extern int verbose ;
extern string curline ;
extern double start ;
extern int quarter ;
extern ll maxmem ;
#define UTIL_H
#endif
