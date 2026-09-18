#ifndef SERVE_H
/*
 *   Serve searches over HTTP on the loopback interface, so that a web page
 *   can use this twsearch instead of a WebAssembly build of it.  This is the
 *   --serve option; it speaks the same protocol as src/js/twsearch-bridge.mjs
 *   (which needs node), so a page cannot tell the two apart.
 *
 *   The server does not search itself.  It runs this same executable as a
 *   child process for the puzzle being solved, one puzzle at a time, exactly
 *   as the node version does: twsearch keeps a puzzle's pruning tables in
 *   memory for as long as its process lives, and a new puzzle means a new
 *   process.
 *
 *   This file is self-contained: leave it out of the build (see
 *   Makefiles/cpp.Makefile) and the --serve option simply does not exist.
 */
int runserver(const char *self);
#define SERVE_H
#endif
