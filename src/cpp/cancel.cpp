#include "cancel.h"
#include <atomic>
#include <iostream>

#include <condition_variable>
#include <deque>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
using namespace std;
// Scrambles are numbered from 1 in the order they arrive (by the reader
// thread) and in the order they are solved (by beginscramble).
static atomic<long> cancelgen{-1};
static atomic<long> solvegen{0};
static long readgen = 0; // reader thread only
static int startsscramble(const string &line) {
  istringstream is(line);
  string tok;
  is >> tok;
  return tok == "Scramble" || tok == "ScrambleState" || tok == "StartState" ||
         tok == "ScrambleAlg" || tok == "CPOS";
}
// A stream buffer fed a line at a time by the reader thread.
class linequeuebuf : public streambuf {
public:
  void push(string s) {
    {
      lock_guard<mutex> l(m);
      q.push_back(std::move(s));
    }
    cv.notify_one();
  }
  void finish() {
    {
      lock_guard<mutex> l(m);
      eof = true;
    }
    cv.notify_one();
  }

protected:
  int_type underflow() override {
    if (gptr() < egptr())
      return traits_type::to_int_type(*gptr());
    unique_lock<mutex> l(m);
    cv.wait(l, [&] { return !q.empty() || eof; });
    if (q.empty())
      return traits_type::eof();
    cur = std::move(q.front());
    q.pop_front();
    setg(&cur[0], &cur[0], &cur[0] + cur.size());
    return traits_type::to_int_type(*gptr());
  }

private:
  mutex m;
  condition_variable cv;
  deque<string> q;
  bool eof = false;
  string cur;
};
istream *cancelablestdin() {
  static linequeuebuf *buf = new linequeuebuf();
  static istream *is = new istream(buf);
  thread([] {
    string line;
    while (getline(cin, line)) {
      if (line.size() && line[line.size() - 1] == '\r')
        line.pop_back();
      if (line == "!cancel") {
        cancelgen.store(readgen);
      } else {
        if (startsscramble(line))
          readgen++;
        buf->push(line + "\n");
      }
    }
    buf->finish();
  }).detach();
  return is;
}
/*
 *   These two are weak, so that a build where a cancel arrives some other
 *   way can define its own and have them called directly: the WebAssembly
 *   build does (see wasm/wasmapi.cpp), where the search has to ask the page
 *   rather than read standard input.  A direct call matters there, since
 *   asking the page suspends the search.
 */
__attribute__((weak)) void beginscramble() { solvegen.fetch_add(1); }
__attribute__((weak)) int searchcanceled() {
  return cancelgen.load(memory_order_relaxed) ==
         solvegen.load(memory_order_relaxed);
}
