#include "util.h"
#include <random>
#include <iostream>
#include <cstdlib>
#include <sys/time.h>
#include <filesystem>
double start ;
int verbose ;
ll maxmem ;
int quarter ;
int quiet ;
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
static mt19937 *rng ;
// avoid static initialization fiasco by always seeding.
void mysrand(int n) {
   if (rng)
      delete rng ;
   rng = new mt19937 ;
   rng->seed(n) ;
}
double myrand(int n) {
   // the following double is exact
   static double mul = 1.0 / (rng->max() - rng->min() + 1.0) ;
   return (int)(((*rng)()-rng->min()) * mul * n) ;
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

#ifdef WASM
string prune_table_path(string _file_name, bool _create_dirs) {
   // Mark inputs as used: https://stackoverflow.com/a/1486931
   (void)_file_name;
   (void)_create_dirs;
   return "BOGUS_PATH";
}
#else
std::filesystem::path cache_home() {
   const char* data_home = std::getenv("XDG_CACHE_HOME");
   if (data_home == NULL) {
      #ifdef _WIN32
         // https://learn.microsoft.com/en-us/windows/deployment/usmt/usmt-recognized-environment-variables#variables-that-are-recognized-only-in-the-user-context
         string data_home(std::getenv("CSIDL_DEFAULT_LOCAL_APPDATA"));
         return data_home;
      #elif __APPLE__
         std::filesystem::path home_path = std::getenv("HOME");
         return home_path / "Library/Caches";
      #else
         // > $XDG_CACHE_HOME defines the base directory relative to which
         // > user-specific non-essential data files should be stored. If
         // > $XDG_CACHE_HOME is either not set or empty, a default equal to
         // > $HOME/.cache should be used.
         // https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html
         std::filesystem::path home_path = std::getenv("HOME");
         return home_path / ".cache";
      #endif
   }
   return std::filesystem::path(data_home);
}
std::filesystem::path app_cache_home(string app_name) {
   return cache_home() / app_name;
}
string prune_table_path(string file_name, bool create_dirs) {
   std::filesystem::path prune_table_dir = app_cache_home("twsearch") / "prune_tables";
   if (create_dirs) {
      std::filesystem::create_directories(prune_table_dir);
   }
   std::filesystem::path prune_table_path = prune_table_dir / file_name;
   return prune_table_path.string(); // The `.string()` call is needed for Windows.
}
#endif
