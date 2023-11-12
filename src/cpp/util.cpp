#include "util.h"
#include <cstdio> // for strerror
#include <cstdlib>
#include <iostream>
#include <random>
#ifdef _WIN64
#include <direct.h> // EEXIST
#else
#include <sys/stat.h> // EEXIST, mkdir
#include <sys/time.h>
#endif
double start;
int verbose;
ll maxmem;
int quarter;
int quiet;
double walltime() {
#ifdef _WIN64
  return GetTickCount() / 1000.0;
#else
  struct timeval tv;
  gettimeofday(&tv, 0);
  return tv.tv_sec + 0.000001 * tv.tv_usec;
#endif
}
double duration() {
  double now = walltime();
  double r = now - start;
  start = now;
  return r;
}
string curline;
void error(string msg, string extra) {
  cerr << msg << extra << endl;
  if (curline.size() > 0)
    cerr << "At: " << curline << endl;
  exit(10);
}
void warn(string msg, string extra) { cerr << msg << extra << endl; }
static mt19937 *rng;
// avoid static initialization fiasco by always seeding.
void mysrand(int n) {
  if (rng)
    delete rng;
  rng = new mt19937;
  rng->seed(n);
}
double myrand(int n) {
  // the following double is exact
  static double mul = 1.0 / ((rng->max)() - (rng->min)() + 1.0);
  return (int)(((*rng)() - (rng->min)()) * mul * n);
}
ull gcd(ull a, ull b) {
  if (a > b)
    swap(a, b);
  if (a == 0)
    return b;
  return gcd(b % a, a);
}
ull lcm(ull a, ull b) { return a / gcd(a, b) * b; }
int ceillog2(int v) {
  int r = 0;
  while (v > (1 << r))
    r++;
  return r;
}
void init_util() { duration(); }
int isprime(int p) {
  if (p < 2)
    return 0;
  if (p < 4)
    return 1;
  if ((p & 1) == 0)
    return 0;
  for (int j = 3;; j += 2) {
    if (p % j == 0)
      return 0;
    if (j * j > p)
      return 1;
  }
}

/*
 *   The cache directory implementation here makes no explicit support
 *   for Unicode.  It should always work for ASCII data; it may work in
 *   UTF-8 or codepage environments, or it may not.
 *
 *   The actual_cache_dir string maintains the storage for the actual
 *   string, and a const char* into the string is what's returned (by
 *   the c_str() method which ensures the terminating 0).
 */
string actual_cache_dir;
const char *user_option_cache_dir;
#ifdef WASM
const char *prune_table_dir(bool _create_dirs) {
  (void)_create_dirs; // Avoid a build warning for the unused arg.
  return "BOGUS_PATH";
}
#else
static int attempted_mkdirs = 0;
#ifdef _WIN32
static const char *envname = "LOCALAPPDATA";
// on Windows, LOCALAPPDATA should always be set, so this fallback should
// never be used.
static const char *defaultdir = "~/.cache/";
#else
static const char *envname = "XDG_CACHE_HOME"; // Mac and Linux
#ifdef __APPLE__
static const char *defaultdir = "~/Library/Caches/";
#else // assume a Unix-like operating system
static const char *defaultdir = "~/.cache/";
#endif
#endif
const char *prune_table_dir(bool createdirs) {
  // do this work only once, but retry if createdirs is 1 and we haven't
  // tried it with createdirs before.  This means when writing a pruning
  // table we actually do this twice.
  if (actual_cache_dir.size() && (!createdirs || attempted_mkdirs))
    return actual_cache_dir.c_str();
  const char *fromenv = 0;
  // if we get system defaults, append the twsearch app name.
  // if we got it from a command line argument, do not.
  int append_app_name = 1;
  if (user_option_cache_dir) {
    fromenv = user_option_cache_dir;
    append_app_name = 0;
  } else {
    fromenv = std::getenv(envname);
    if (fromenv == 0)
      fromenv = defaultdir;
  }
  if (fromenv == 0 || *fromenv == 0)
    error("! cannot determine cache directory to write pruning table");
  string cachedir(fromenv, fromenv + strlen(fromenv));
#ifndef _WIN32
  // on MacOS and Windows, expand tilde, but only if the HOME
  // environment variable is set.
  if (cachedir[0] == '~' && cachedir[1] == '/') {
    const char *homedir = getenv("HOME");
    if (homedir != 0) {
      // only remove the ~, but retain the directory separator
      cachedir.erase(cachedir.begin(), cachedir.begin() + 1);
      cachedir.insert(cachedir.begin(), homedir, homedir + strlen(homedir));
    } else {
      error("! could not expand leading tilde in config path using HOME "
            "environment variable");
    }
  }
#endif
  int created_dir = 0;
  while (1) {
    // ensure final character is not a slash (or, on Windows, a backslash).
    // do this to protect against mkdir's that don't handle trailing slashes.
    int lastc = cachedir[cachedir.size() - 1];
#ifdef _WIN32
    if (lastc == '/' || lastc == '\\')
#else
    if (lastc == '/')
#endif
    {
      cachedir.pop_back();
    }
    // make the directory if it doesn't exist, but only if so requested.
    if (createdirs) {
#ifdef _WIN32
      int rc = _mkdir(cachedir.c_str());
#else
      int rc = mkdir(cachedir.c_str(), 0777);
#endif
      if (rc != 0 && errno != EEXIST) {
        cerr << "Error while creating directory " << cachedir << " was "
             << strerror(errno) << endl;
        error("! could not create requested cache dir");
      }
      created_dir |= (rc == 0);
    }
    cachedir.push_back('/');
    if (append_app_name) {
      cachedir += "twsearch";
      append_app_name = 0;
    } else {
      // nothing more to do; return name.
      break;
    }
  }
  // if we actually created one of the directories, write a README.txt
  // file in case someone wanders around the filesystem and bumps into
  // a bunch of huge files.
  if (created_dir) {
    string readmename = cachedir + "readme.txt";
    FILE *f = fopen(readmename.c_str(), "r");
    if (f == 0) { // only write if we won't smash an existing file
      f = fopen(readmename.c_str(), "w");
      if (f == 0)
        error("! couldn't open readme.txt in cache dir");
      fprintf(
          f,
          R"(Files in this directory are temporary pruning table files created by
the twsearch twisty puzzle searching program.  They may be safely
deleted because they will be recreated as needed.  For more information
see

   https://github.com/cubing/twsearch/
)");
    }
    fclose(f);
  }
  swap(actual_cache_dir, cachedir);
  return actual_cache_dir.c_str();
}
#endif
