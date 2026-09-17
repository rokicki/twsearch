/*
 *   WebAssembly entry points for twsearch.
 *
 *   One module instance holds one puzzle and its pruning table, so that
 *   successive solves of the same puzzle reuse the table.  To change
 *   puzzles or options the caller makes a new instance; much of twsearch's
 *   state is global and we do not try to reset it.
 *
 *   The protocol mirrors the command line with scrambles on standard input
 *   (twsearch [options] puzzle.tws -).  w_args takes the options
 *   (whitespace separated), w_setksolve takes the contents of a .tws file,
 *   and w_solvescramble takes scramble text.  Everything twsearch prints goes
 *   to stdout exactly as the native binary prints it.
 *
 *   Output does not go through emscripten's print/printErr, which only see
 *   whole lines.  Each time twsearch flushes cout or cerr (or fills the
 *   buffer), the text is passed to Module.twsearchOutput(fd, text), with fd
 *   1 or 2: the same pieces a native twsearch writes to stdout and stderr,
 *   including partial lines such as "Filling depth 7 val 2" before a long
 *   table fill.
 *
 *   w_solvescramble is asynchronous (Asyncify): call it with
 *   ccall(..., {async: true}) and await the result.  While it runs, setting
 *   Module.twsearchCanceled = true cancels the scramble being solved (see
 *   cancel.h).
 *
 *   On an error, where the native binary would print the message to stderr
 *   and exit, these functions print the message to stderr and throw a
 *   JavaScript Error named TwsearchError.  The instance must then be
 *   discarded, just as the native process would be gone.
 */
#include "../prunetable.h"
#include "../puzdef.h"
#include "../twsearch.h"
#include "../util.h"
#include <emscripten/emscripten.h>
#include <iostream>
#include <sstream>
#include <streambuf>
#include <vector>

static puzdef *wasm_pd;
static prunetable *wasm_pt;

// The body is JavaScript.
// clang-format off
EM_JS(void, js_output, (int fd, const char *text, int len), {
  // Pointers arrive as BigInt in a 64-bit (Memory64) build.
  var s = UTF8ToString(Number(text), len);
  if (Module.twsearchOutput)
    Module.twsearchOutput(fd, s);
  else
    (fd == 2 ? err : out)(s);
});
// clang-format on

// A stream buffer that hands its contents to JavaScript when flushed.
class jsoutbuf : public streambuf {
public:
  explicit jsoutbuf(int fd_) : fd(fd_) { setp(buf, buf + sizeof(buf)); }

protected:
  int_type overflow(int_type c) override {
    emit();
    if (c != traits_type::eof()) {
      *pptr() = traits_type::to_char_type(c);
      pbump(1);
    }
    return traits_type::not_eof(c);
  }
  int sync() override {
    emit();
    return 0;
  }

private:
  void emit() {
    if (pptr() > pbase())
      js_output(fd, pbase(), pptr() - pbase());
    setp(buf, buf + sizeof(buf));
  }
  int fd;
  char buf[4096];
};

static void wasm_init() {
  static int inited = 0;
  if (!inited) {
    static jsoutbuf outbuf(1), errbuf(2);
    cout.rdbuf(&outbuf);
    cerr.rdbuf(&errbuf);
    inited = 1;
  }
}

extern "C" {

EMSCRIPTEN_KEEPALIVE void w_args(const char *s) {
  wasm_init();
  reseteverything();
  // processargs wants argv[0] to be the program name, and options such
  // as --moves keep pointers into argv, so the strings are never freed.
  vector<const char *> argv;
  argv.push_back("twsearch");
  istringstream is(s);
  string tok;
  while (is >> tok)
    argv.push_back(strdup(tok.c_str()));
  argv.push_back(0);
  int argc = argv.size() - 1;
  const char **argvp = argv.data();
  processargs(argc, argvp, 0);
  if (argc != 1)
    error("! unexpected non-option argument ", argvp[1]);
}

EMSCRIPTEN_KEEPALIVE void w_setksolve(const char *s) {
  wasm_init();
  if (wasm_pd)
    error("! puzzle already set; make a new instance to change puzzles");
  wasm_pd = new puzdef(makepuzdef(string(s)));
  cout << flush;
}

EMSCRIPTEN_KEEPALIVE void w_solvescramble(const char *s) {
  wasm_init();
  if (!wasm_pd)
    error("! you must set the puzzle definition before solving");
  istringstream is(s);
  // builds the pruning table on the first scramble, then keeps it
  processscrambles(&is, *wasm_pd, wasm_pt, gs);
  cout << flush;
}
}
