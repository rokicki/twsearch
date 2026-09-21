#include "serve.h"
#include "cmds.h"
#include "twsearch.h"
#include "util.h"
// clang-format off
#include "vendor/cpp-httplib/httplib.h"
#include "vendor/picojson/picojson.h"
// clang-format on
#include <chrono>
#define STR2(x) #x
#define STRINGIZE(x) STR2(x)
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#endif
using namespace std;

static int serveport = 2023;
/*
 *   --serve also answers with a page, so that someone can open
 *   http://127.0.0.1:<port>/ and have Gyrelab.  That page holds nothing but
 *   a script tag: browsers no longer let a page from the open web reach a
 *   program on the reader's own machine, but a page served from here is
 *   already here, and may ask this twsearch for searches with none of that
 *   in the way.  What the page actually is comes from the site below, so it
 *   is always the current version and this needs to know one address and
 *   nothing else about it.  --app points somewhere else (a copy being worked
 *   on, say), and --no-app serves no page at all.
 */
static string appurl = "https://cube20.org/gyrelab/";
static vector<string> alloworigins = {
    "https://alpha.twizzle.net",
    "https://experiments.cubing.net",
    "https://cube20.org",
    "https://www.cube20.org",
};
// What a page may ask for.  Anything else is refused: this is a program on
// someone's own machine being driven by a web page, so it only does the one
// thing, with the options that make sense for it.
static const struct {
  const char *option;
  int values;
} allowedargs[] = {
    {"-c", 1},
    {"-M", 1},
    {"-t", 1},
    {"-R", 1},
    {"--moves", 1},
    {"--mindepth", 1},
    {"--maxdepth", 1},
    {"--startprunedepth", 1},
    {"--microthreads", 1},
    {"--newcanon", 1},
    {"--omit", 1},
    {"--omitperms", 1},
    {"--omitoris", 1},
    {"--orientationgroup", 1},
    {"--writeprunetables", 1},
    {"-q", 0},
    {"--quiet", 0},
    {"--alloptimal", 0},
    {"--randomstart", 0},
    {"--checkbeforesolve", 0},
    {"--noearlysolutions", 0},
    {"--nosymmetry", 0},
    {"--nocorners", 0},
    {"--nocenters", 0},
    {"--noedges", 0},
    {"--noorientation", 0},
    {"--distinguishall", 0},
    {"--nowrite", 0},
};

/*
 *   Running this executable as a child, with pipes.  Two platforms, one
 *   interface; everything above this is the same on both.
 */
struct childproc {
#ifdef _WIN32
  HANDLE pid = NULL, in = NULL, out = NULL, err = NULL;
#else
  pid_t pid = -1;
  int in = -1, out = -1, err = -1;
#endif
  bool start(const string &exe, const vector<string> &args, string &failure);
  // Returns the number of bytes read, or 0 at end of output.
  int readsome(bool stderrpipe, char *buf, int len);
  void write(const string &s);
  void closeinput();
  void killit();
  // Waits for the child and says how it ended, for an error message.
  string wait();
};

#ifdef _WIN32
static string quoteforwindows(const string &arg) {
  // Command lines are one string on Windows; quote what the C runtime of the
  // child will split again.
  if (arg.find_first_of(" \t\"") == string::npos)
    return arg;
  string r = "\"";
  int backslashes = 0;
  for (char c : arg) {
    if (c == '\\') {
      backslashes++;
    } else if (c == '"') {
      r.append(2 * backslashes + 1, '\\');
      backslashes = 0;
    } else {
      r.append(backslashes, '\\');
      backslashes = 0;
    }
    if (c != '\\')
      r.push_back(c);
  }
  r.append(2 * backslashes, '\\');
  return r + "\"";
}

bool childproc::start(const string &exe, const vector<string> &args,
                      string &failure) {
  SECURITY_ATTRIBUTES sa;
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;
  sa.lpSecurityDescriptor = NULL;
  HANDLE inread = NULL, outwrite = NULL, errwrite = NULL;
  if (!CreatePipe(&inread, &in, &sa, 0) ||
      !CreatePipe(&out, &outwrite, &sa, 0) ||
      !CreatePipe(&err, &errwrite, &sa, 0)) {
    failure = "could not create pipes";
    return false;
  }
  SetHandleInformation(in, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(out, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(err, HANDLE_FLAG_INHERIT, 0);
  string cmdline = quoteforwindows(exe);
  for (const auto &a : args)
    cmdline += " " + quoteforwindows(a);
  STARTUPINFOA si;
  memset(&si, 0, sizeof(si));
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = inread;
  si.hStdOutput = outwrite;
  si.hStdError = errwrite;
  PROCESS_INFORMATION pi;
  memset(&pi, 0, sizeof(pi));
  vector<char> mutablecmd(cmdline.begin(), cmdline.end());
  mutablecmd.push_back(0);
  BOOL ok = CreateProcessA(NULL, mutablecmd.data(), NULL, NULL, TRUE, 0, NULL,
                           NULL, &si, &pi);
  CloseHandle(inread);
  CloseHandle(outwrite);
  CloseHandle(errwrite);
  if (!ok) {
    failure = "could not run " + exe;
    return false;
  }
  CloseHandle(pi.hThread);
  pid = pi.hProcess;
  return true;
}

int childproc::readsome(bool stderrpipe, char *buf, int len) {
  HANDLE h = stderrpipe ? err : out;
  DWORD got = 0;
  if (h == NULL || !ReadFile(h, buf, len, &got, NULL))
    return 0;
  return (int)got;
}

void childproc::write(const string &s) {
  DWORD wrote = 0;
  if (in != NULL)
    WriteFile(in, s.data(), (DWORD)s.size(), &wrote, NULL);
}

void childproc::closeinput() {
  if (in != NULL) {
    CloseHandle(in);
    in = NULL;
  }
}

void childproc::killit() {
  if (pid != NULL)
    TerminateProcess(pid, 1);
}

string childproc::wait() {
  if (pid == NULL)
    return "twsearch did not start";
  WaitForSingleObject(pid, INFINITE);
  DWORD code = 0;
  GetExitCodeProcess(pid, &code);
  CloseHandle(pid);
  pid = NULL;
  if (out != NULL) {
    CloseHandle(out);
    out = NULL;
  }
  if (err != NULL) {
    CloseHandle(err);
    err = NULL;
  }
  closeinput();
  if (code == 0)
    return "";
  return "twsearch exited with code " + to_string((int)code);
}
#else
bool childproc::start(const string &exe, const vector<string> &args,
                      string &failure) {
  int inpipe[2], outpipe[2], errpipe[2];
  if (pipe(inpipe) || pipe(outpipe) || pipe(errpipe)) {
    failure = "could not create pipes";
    return false;
  }
  vector<char *> argv;
  argv.push_back(const_cast<char *>(exe.c_str()));
  for (const auto &a : args)
    argv.push_back(const_cast<char *>(a.c_str()));
  argv.push_back(NULL);
  pid = fork();
  if (pid < 0) {
    failure = "could not fork";
    return false;
  }
  if (pid == 0) {
    dup2(inpipe[0], 0);
    dup2(outpipe[1], 1);
    dup2(errpipe[1], 2);
    close(inpipe[0]);
    close(inpipe[1]);
    close(outpipe[0]);
    close(outpipe[1]);
    close(errpipe[0]);
    close(errpipe[1]);
    execv(exe.c_str(), argv.data());
    // Only reached if exec failed; the parent sees this on stderr.
    fprintf(stderr, "! could not run %s\n", exe.c_str());
    _exit(127);
  }
  close(inpipe[0]);
  close(outpipe[1]);
  close(errpipe[1]);
  in = inpipe[1];
  out = outpipe[0];
  err = errpipe[0];
  return true;
}

int childproc::readsome(bool stderrpipe, char *buf, int len) {
  int fd = stderrpipe ? err : out;
  if (fd < 0)
    return 0;
  int got;
  do {
    got = (int)read(fd, buf, len);
  } while (got < 0 && errno == EINTR);
  return got > 0 ? got : 0;
}

void childproc::write(const string &s) {
  if (in >= 0) {
    ssize_t ignored = ::write(in, s.data(), s.size());
    (void)ignored;
  }
}

void childproc::closeinput() {
  if (in >= 0) {
    close(in);
    in = -1;
  }
}

void childproc::killit() {
  if (pid > 0)
    ::kill(pid, SIGKILL);
}

string childproc::wait() {
  if (pid <= 0)
    return "twsearch did not start";
  int status = 0;
  while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
    ;
  pid = -1;
  if (out >= 0) {
    close(out);
    out = -1;
  }
  if (err >= 0) {
    close(err);
    err = -1;
  }
  closeinput();
  if (WIFSIGNALED(status))
    return "twsearch killed (signal " + to_string(WTERMSIG(status)) + ")";
  if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
    return "twsearch exited with code " + to_string(WEXITSTATUS(status));
  return "";
}
#endif

/*
 *   --echo writes what crosses the bridge to standard output: the puzzle and
 *   the scrambles as they arrive, and the solver's output as it goes back.
 *   The output is written as it is and in the pieces it arrives in, so a
 *   line the solver is still writing ("Filling depth 8 val 2 ") shows here
 *   when it shows on the page, and not when it ends.  The puzzle and the
 *   scrambles are written as they are, so those blocks can be lifted out as
 *   a .tws file and run again.  The lines this adds to say
 *   what is going on are comments, which is what makes lifting a block out
 *   simple, but the whole transcript is not a .tws file and is not meant to
 *   be one.
 */
static int echosearches = 0;
static mutex echolock;
static bool echoatlinestart = true;

static void echowrite(const string &text) { // echolock held
  cout << text << flush;
  echoatlinestart = text.back() == '\n';
}

static void echotext(const string &text) {
  if (!echosearches || text.empty())
    return;
  lock_guard<mutex> hold(echolock);
  echowrite(text);
}

// The transcript's own remarks about what is happening, as comments so
// that a puzzle or scramble block can be lifted out of it cleanly.
static void echocomment(const string &line) {
  if (!echosearches)
    return;
  lock_guard<mutex> hold(echolock);
  if (!echoatlinestart) // never land in the middle of the solver's line
    echowrite("\n");
  echowrite("# " + line + "\n");
}

/*
 *   One position to solve: what the page asked for, and the output on its
 *   way back.  The HTTP thread waits on these; the threads reading the
 *   child fill them in.
 */
struct jobrec {
  string id;
  string scramble;
  deque<string> events; // NDJSON lines, ready to send
  bool finished = false;
  string failure; // empty when the search simply ended
};
typedef shared_ptr<jobrec> jobptr;

static mutex servelock;
static condition_variable servewake;

static string jsonline(const string &type, const string &field,
                       const string &value) {
  picojson::object o;
  o["type"] = picojson::value(type);
  if (field.size())
    o[field] = picojson::value(value);
  return picojson::value(o).serialize() + "\n";
}

/*
 *   The child twsearch for one puzzle and set of options, and the jobs it
 *   is working through.  Everything here runs under servelock.
 */
struct childrun : enable_shared_from_this<childrun> {
  string key;
  childproc proc;
  vector<jobptr> queue;
  jobptr current;
  deque<string> stderrtail;
  string outpartial;
  bool dead = false;
  string killreason;
  string spawnfailure;
  thread outreader, errreader, reaper;

  void onstdout(const string &text);
  void onstdoutline(const string &line);
  void onstderr(const string &text);
  void onclose(const string &failure);
  void addjob(const jobptr &job);
  void next();
  void cancel(const jobptr &job, bool abandon);
  jobptr findjob(const string &id);
  void killit(const string &reason);
};

static shared_ptr<childrun> running;
static string selfpath;
static filesystem::path workdir;

// twsearch says this when it is done with one position (see solve.cpp).
static bool endofsolve(const string &line) {
  static const char *enders[] = {"Found ", "No solution found in ",
                                 "Ignoring unsolvable position.",
                                 "Search canceled at depth "};
  for (const char *e : enders)
    if (line.compare(0, strlen(e), e) == 0) {
      // "Found 3 solutions" and the like, not "Found 13 canonical move states".
      if (strcmp(e, "Found ") == 0 && line.find(" solution") == string::npos)
        continue;
      return true;
    }
  return false;
}

void childrun::onstdout(const string &text) {
  lock_guard<mutex> hold(servelock);
  echotext(text);
  if (current)
    current->events.push_back(jsonline("out", "text", text));
  outpartial += text;
  size_t at;
  while ((at = outpartial.find('\n')) != string::npos) {
    string line = outpartial.substr(0, at);
    outpartial.erase(0, at + 1);
    onstdoutline(line);
  }
  servewake.notify_all();
}

void childrun::onstdoutline(const string &line) {
  if (!current || !endofsolve(line))
    return;
  jobptr job = current;
  current = nullptr;
  job->events.push_back(jsonline("done", "", ""));
  job->finished = true;
  next();
}

void childrun::onstderr(const string &text) {
  lock_guard<mutex> hold(servelock);
  if (current)
    current->events.push_back(jsonline("err", "text", text));
  echotext(text);
  stderrtail.push_back(text);
  while (stderrtail.size() > 20)
    stderrtail.pop_front();
  servewake.notify_all();
}

void childrun::onclose(const string &failure) {
  lock_guard<mutex> hold(servelock);
  dead = true;
  if (running.get() == this)
    running = nullptr;
  vector<jobptr> all;
  if (current)
    all.push_back(current);
  for (auto &job : queue)
    all.push_back(job);
  current = nullptr;
  queue.clear();
  if (echosearches && all.size())
    echocomment("error: " + failure);
  for (auto &job : all) {
    job->events.push_back(jsonline("error", "message", failure));
    job->failure = failure;
    job->finished = true;
  }
  servewake.notify_all();
}

void childrun::addjob(const jobptr &job) {
  queue.push_back(job);
  next();
}

void childrun::next() {
  if (dead || current || queue.empty())
    return;
  current = queue.front();
  queue.erase(queue.begin());
  stderrtail.clear();
  string text = current->scramble;
  if (text.empty() || text.back() != '\n')
    text += "\n";
  if (echosearches) {
    echocomment("");
    echocomment("solve " + current->id);
    echotext(text);
  }
  proc.write(text);
}

void childrun::cancel(const jobptr &job, bool abandon) {
  for (size_t i = 0; i < queue.size(); i++)
    if (queue[i] == job) {
      queue.erase(queue.begin() + i);
      job->events.push_back(
          jsonline("error", "message", "canceled before it started"));
      job->finished = true;
      servewake.notify_all();
      return;
    }
  if (current == job) {
    if (abandon)
      killit("abandoned");
    else
      proc.write("!cancel\n");
  }
}

jobptr childrun::findjob(const string &id) {
  if (id.empty())
    return current;
  if (current && current->id == id)
    return current;
  for (auto &job : queue)
    if (job->id == id)
      return job;
  return nullptr;
}

void childrun::killit(const string &reason) {
  if (killreason.empty())
    killreason = reason;
  proc.killit();
}

static shared_ptr<childrun> startchild(const string &key, const string &tws,
                                       const vector<string> &args) {
  auto run = make_shared<childrun>();
  run->key = key;
  // twsearch takes the puzzle from a file, so write one.  Its name matters:
  // twsearch names a puzzle's pruning tables after the part before the first
  // dot, and reuses them when it sees that puzzle again.
  string name = "puzzle";
  size_t at = tws.find("Name ");
  if (at != string::npos && (at == 0 || tws[at - 1] == '\n')) {
    size_t end = tws.find_first_of("\r\n", at);
    name = tws.substr(at + 5, end - at - 5);
    for (auto &c : name)
      if (!isalnum((unsigned char)c) && c != '_' && c != '-')
        c = '_';
    if (name.empty())
      name = "puzzle";
  }
  filesystem::path twsfile = workdir / (name + ".tws");
  {
    ofstream f(twsfile, ios::binary | ios::trunc);
    f << tws;
  }
  // The server's own pruning table settings come first, so the page's
  // options, parsed after them, can override --writeprunetables.
  static const char *writenames[] = {"never", "auto", "always"};
  vector<string> childargs = {"--writeprunetables",
                              writenames[writeprunetables]};
  if (user_option_cache_dir) {
    childargs.push_back("--cachedir");
    childargs.push_back(user_option_cache_dir);
  }
  childargs.insert(childargs.end(), args.begin(), args.end());
  childargs.push_back(twsfile.string());
  childargs.push_back("-"); // read positions to solve from standard input
  if (echosearches) {
    string cmd = "twsearch";
    for (const auto &a : childargs)
      cmd += " " + abbreviatehome(a); // this line is going somewhere
    echocomment("");
    echocomment(cmd);
    echotext(tws.size() && tws.back() == '\n' ? tws : tws + "\n");
  }
  if (!run->proc.start(selfpath, childargs, run->spawnfailure)) {
    run->dead = true;
    return run;
  }
  auto readpipe = [](shared_ptr<childrun> self, bool stderrpipe) {
    char buf[65536];
    while (1) {
      int got = self->proc.readsome(stderrpipe, buf, sizeof(buf));
      if (got <= 0)
        break;
      string text(buf, got);
      if (stderrpipe)
        self->onstderr(text);
      else
        self->onstdout(text);
    }
  };
  run->outreader = thread(readpipe, run, false);
  run->errreader = thread(readpipe, run, true);
  run->reaper = thread(
      [](shared_ptr<childrun> self) {
        self->outreader.join();
        self->errreader.join();
        string how = self->proc.wait();
        string failure;
        {
          lock_guard<mutex> hold(servelock);
          if (!self->killreason.empty())
            failure = self->killreason;
          else if (!self->spawnfailure.empty())
            failure = self->spawnfailure;
          else if (!self->stderrtail.empty()) {
            for (const auto &s : self->stderrtail)
              failure += s;
            while (failure.size() &&
                   (failure.back() == '\n' || failure.back() == '\r'))
              failure.pop_back();
          } else
            failure = how.empty() ? "twsearch stopped" : how;
        }
        self->onclose(failure);
      },
      run);
  // Nothing waits for these: the reader threads end when the child's output
  // does, and the thread that reaps it finishes the jobs it had.  Each holds
  // a reference, so the run lives exactly as long as it has work to do.
  run->reaper.detach();
  return run;
}

/*
 *   The twsearch for this puzzle and these options, starting one (and
 *   stopping the one before) when the puzzle or the options change.
 */
static shared_ptr<childrun> getchild(const string &tws,
                                     const vector<string> &args) {
  string key;
  for (const auto &a : args)
    key += a + "\n";
  key += "\n" + tws;
  if (running && !running->dead && running->key == key)
    return running;
  if (running) {
    // Its own threads tidy it up once it dies, and its jobs are told why.
    running->killit("puzzle changed");
    running = nullptr;
  }
  running = startchild(key, tws, args);
  return running;
}

/* Only the options a page may ask for, and never more memory than allowed. */
static bool checkargs(const picojson::value &given, vector<string> &out,
                      string &failure) {
  bool sawmem = false;
  ll memcap = maxmem / 1048576;
  if (given.is<picojson::array>()) {
    const auto &list = given.get<picojson::array>();
    for (size_t i = 0; i < list.size(); i++) {
      if (!list[i].is<string>()) {
        failure = "options must be strings";
        return false;
      }
      string option = list[i].get<string>();
      int values = -1;
      // -v, -v0 ... -v9 say how much to print.
      if (option.compare(0, 2, "-v") == 0 &&
          (option.size() == 2 ||
           (option.size() == 3 && isdigit((unsigned char)option[2]))))
        values = 0;
      for (const auto &a : allowedargs)
        if (option == a.option)
          values = a.values;
      if (values < 0) {
        failure = "option not allowed through the server: " + option;
        return false;
      }
      out.push_back(option);
      if (values == 1) {
        if (i + 1 >= list.size() || !list[i + 1].is<string>()) {
          failure = option + " needs a value";
          return false;
        }
        string value = list[++i].get<string>();
        if (option == "-M") {
          sawmem = true;
          ll mb = atoll(value.c_str());
          if (mb <= 0) {
            failure = "bad -M value";
            return false;
          }
          value = to_string(min(mb, memcap));
        }
        out.push_back(value);
      }
    }
  }
  if (!sawmem) {
    out.push_back("-M");
    out.push_back(to_string(memcap));
  }
  return true;
}

static bool originallowed(const string &origin) {
  if (origin.empty())
    return true; // not a browser
  /*
   *   "null" is not trusted here, though a page opened from a file sends it.
   *   So does a page in a sandboxed iframe, which any site on the web can
   *   put on any page it likes: trusting "null" hands every one of them a
   *   solver on this machine to drive.  --allow-origin null says to take
   *   that on anyway, for someone running a downloaded page who knows what
   *   it costs; the page twsearch serves itself needs none of it.
   */
  for (const auto &allowed : alloworigins)
    if (origin == allowed)
      return true;
  // A page served from this machine, however it was started.
  for (const char *prefix :
       {"http://localhost", "https://localhost", "http://127.0.0.1",
        "https://127.0.0.1", "http://[::1]", "https://[::1]"}) {
    size_t n = strlen(prefix);
    if (origin.compare(0, n, prefix) == 0 &&
        (origin.size() == n || origin[n] == ':'))
      return true;
  }
  return false;
}

// Guards against DNS rebinding: the page must have addressed us by a name
// that means this machine.
static bool hostallowed(const string &host) {
  string h = host.substr(0, host.rfind(':'));
  if (h.empty())
    h = host;
  return h == "127.0.0.1" || h == "localhost" || h == "[::1]";
}

/*
 *   A search should not outlive the server that asked for it.  The server
 *   puts its own process id in the environment its children inherit; a
 *   child that finds it there watches that process and exits when it goes
 *   away, so killing the server does not leave a search running with
 *   nobody to read its answer and gigabytes of pruning table held.  This is
 *   one thread that spends its life asleep rather than a check in the
 *   search, which would cost something on every node; the search is stopped
 *   by ending the process, since there is nothing left to report to.
 *
 *   Neither side can do this alone on all three platforms: Linux has
 *   PR_SET_PDEATHSIG and Windows has job objects, but macOS has no way for
 *   a parent to arrange it, so the child watches instead, the same way
 *   everywhere.
 */
static const char *serverpidvar = "TWSEARCH_SERVER_PID";

static void tellchildrenwhoweare() {
  string pid = to_string((long long)
#ifdef _WIN32
                             GetCurrentProcessId()
#else
                             getpid()
#endif
  );
#ifdef _WIN32
  _putenv_s(serverpidvar, pid.c_str());
#else
  setenv(serverpidvar, pid.c_str(), 1);
#endif
}

void watchparent() {
  const char *s = getenv(serverpidvar);
  if (s == 0 || *s == 0)
    return;
  long long serverpid = atoll(s);
  if (serverpid <= 0)
    return;
  thread([serverpid]() {
#ifdef _WIN32
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)serverpid);
    if (h == NULL) // cannot watch it; better to search on than to stop
      return;
    WaitForSingleObject(h, INFINITE);
#else
    // Cheap enough once a second in a thread that is otherwise asleep; a
    // second is prompt for letting go of the memory.
    while (getppid() == (pid_t)serverpid)
      this_thread::sleep_for(chrono::seconds(1));
#endif
    _Exit(0);
  }).detach();
}

int runserver(const char *self) {
  selfpath = self;
  tellchildrenwhoweare();
#ifdef _WIN32
  // --echo promises the bytes the page was sent.  Standard output here is
  // in text mode, which turns every \n into \r\n; what the solver wrote
  // already ends \r\n, so the transcript would carry \r\r\n.
  if (echosearches)
    _setmode(_fileno(stdout), _O_BINARY);
#endif
#ifndef _WIN32
  // The child's standard input closing is normal; do not die of it.
  signal(SIGPIPE, SIG_IGN);
#endif
  // The server runs this same program for each puzzle, so it has to know
  // where it is.  When it was started by name, look along the PATH for it.
  if (!filesystem::exists(selfpath) &&
      selfpath.find_first_of("/\\") == string::npos) {
    const char *path = getenv("PATH");
#ifdef _WIN32
    const char sep = ';';
    const char *suffix = ".exe";
#else
    const char sep = ':';
    const char *suffix = "";
#endif
    for (string rest = path ? path : ""; rest.size();) {
      size_t at = rest.find(sep);
      string dir = rest.substr(0, at);
      rest = at == string::npos ? "" : rest.substr(at + 1);
      filesystem::path candidate = filesystem::path(dir) / (selfpath + suffix);
      if (filesystem::exists(candidate)) {
        selfpath = candidate.string();
        break;
      }
    }
  }
  if (!filesystem::exists(selfpath))
    error("! --serve cannot find this program; run it by its path");
  selfpath = filesystem::absolute(selfpath).string();
  workdir = filesystem::temp_directory_path() /
            ("twsearch-serve-" + to_string(
#ifdef _WIN32
                                     (long long)GetCurrentProcessId()
#else
                                     (long long)getpid()
#endif
                                         ));
  filesystem::create_directories(workdir);

  httplib::Server server;
  server.set_payload_max_length(32 * 1024 * 1024);
  /*
   *   One twsearch to a port.  The default here sets SO_REUSEPORT, which
   *   lets a second server bind the same port and leaves the system to give
   *   each connection to one or the other; a page then cannot tell which one
   *   it is talking to, and stopping either kills whatever it was doing.
   *   SO_REUSEADDR on its own still lets this start again straight after it
   *   is stopped, without waiting for old connections to time out.
   */
  server.set_socket_options([](socket_t sock) {
#ifdef _WIN32
    httplib::set_socket_opt(sock, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, 1);
#else
    httplib::set_socket_opt(sock, SOL_SOCKET, SO_REUSEADDR, 1);
#endif
  });

  auto refuse = [](const httplib::Request &req, httplib::Response &res) {
    if (!hostallowed(req.get_header_value("Host")) ||
        !originallowed(req.get_header_value("Origin"))) {
      res.status = 403;
      res.set_content("forbidden\n", "text/plain");
      return true;
    }
    const auto origin = req.get_header_value("Origin");
    if (origin.size()) {
      res.set_header("Access-Control-Allow-Origin", origin);
      res.set_header("Vary", "Origin");
    }
    return false;
  };

  if (appurl.size()) {
    server.Get("/", [&](const httplib::Request &, httplib::Response &res) {
      res.set_content("<!DOCTYPE html>\n"
                      "<meta charset=\"utf-8\">\n"
                      "<meta name=\"viewport\" content=\"width=device-width, "
                      "initial-scale=0.75\">\n"
                      "<title>Gyrelab</title>\n"
                      "<body>\n"
                      "<p>Fetching Gyrelab from " +
                          appurl + " ...</p>\n<script src=\"" + appurl +
                          "boot.js\"></script>\n",
                      "text/html");
    });
  }

  server.Options(
      "/v1/.*", [&](const httplib::Request &req, httplib::Response &res) {
        if (refuse(req, res))
          return;
        res.set_header("Access-Control-Allow-Methods", "GET, POST");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        // Chrome asks this before a page may talk to a program on this machine.
        res.set_header("Access-Control-Allow-Private-Network", "true");
        res.set_header("Access-Control-Max-Age", "600");
        res.status = 204;
      });

  server.Get(
      "/v1/info", [&](const httplib::Request &req, httplib::Response &res) {
        if (refuse(req, res))
          return;
        picojson::object o;
        o["bridge"] = picojson::value("twsearch-bridge");
        o["protocol"] = picojson::value((double)1);
        // Which twsearch, but not where it lives: the path names the
        // reader's home directory, and nothing needs it.
        o["version"] = picojson::value(string(STRINGIZE(TWSEARCH_VERSION)));
        o["threads"] = picojson::value((double)numthreads);
        o["maxMem"] = picojson::value((double)(maxmem / 1048576));
        res.set_content(picojson::value(o).serialize(), "application/json");
      });

  server.Post("/v1/cancel",
              [&](const httplib::Request &req, httplib::Response &res) {
                if (refuse(req, res))
                  return;
                picojson::value body;
                picojson::parse(body, req.body);
                string id;
                bool abandon = false;
                if (body.is<picojson::object>()) {
                  const auto &o = body.get<picojson::object>();
                  auto idat = o.find("id");
                  if (idat != o.end() && idat->second.is<string>())
                    id = idat->second.get<string>();
                  auto abandonat = o.find("abandon");
                  if (abandonat != o.end() && abandonat->second.is<bool>())
                    abandon = abandonat->second.get<bool>();
                }
                {
                  lock_guard<mutex> hold(servelock);
                  if (running) {
                    jobptr job = running->findjob(id);
                    if (job)
                      running->cancel(job, abandon);
                  }
                }
                res.status = 204;
              });

  server.Post("/v1/solve", [&](const httplib::Request &req,
                               httplib::Response &res) {
    if (refuse(req, res))
      return;
    picojson::value body;
    string parsefailure = picojson::parse(body, req.body);
    if (parsefailure.size() || !body.is<picojson::object>()) {
      res.status = 400;
      res.set_content("body must be JSON\n", "text/plain");
      return;
    }
    const auto &o = body.get<picojson::object>();
    auto twsat = o.find("tws");
    auto scrambleat = o.find("scramble");
    if (twsat == o.end() || !twsat->second.is<string>() ||
        scrambleat == o.end() || !scrambleat->second.is<string>()) {
      res.status = 400;
      res.set_content("tws and scramble must be strings\n", "text/plain");
      return;
    }
    vector<string> args;
    string failure;
    auto argsat = o.find("args");
    if (!checkargs(argsat == o.end() ? picojson::value() : argsat->second, args,
                   failure)) {
      res.status = 400;
      res.set_content(failure + "\n", "text/plain");
      return;
    }
    auto job = make_shared<jobrec>();
    auto idat = o.find("id");
    if (idat != o.end() && idat->second.is<string>())
      job->id = idat->second.get<string>();
    job->scramble = scrambleat->second.get<string>();
    shared_ptr<childrun> run;
    {
      unique_lock<mutex> hold(servelock);
      run = getchild(twsat->second.get<string>(), args);
      run->addjob(job);
    }
    res.set_header("Cache-Control", "no-store");
    res.set_chunked_content_provider(
        "application/x-ndjson", [job, run](size_t, httplib::DataSink &sink) {
          string send;
          bool done = false;
          {
            unique_lock<mutex> hold(servelock);
            servewake.wait(
                hold, [&] { return job->finished || !job->events.empty(); });
            while (!job->events.empty()) {
              send += job->events.front();
              job->events.pop_front();
            }
            done = job->finished;
          }
          if (send.size() && !sink.write(send.data(), send.size())) {
            // The page went away in the middle of a search: stop searching.
            lock_guard<mutex> hold(servelock);
            if (running == run)
              run->cancel(job, false);
            return false;
          }
          if (done)
            sink.done();
          return true;
        });
  });

  // Take the port first, so that what is said next is true.
  if (!server.bind_to_port("127.0.0.1", serveport)) {
    filesystem::remove_all(workdir);
    error("! could not serve on port " + to_string(serveport) +
          "; something is already using it (another twsearch --serve?).  "
          "Stop that one, or give this one a --port of its own.");
  }
  for (const auto &allowed : alloworigins)
    if (allowed == "null")
      warn("--allow-origin null lets any site reach this server, through a "
           "sandboxed iframe, for as long as it runs");
  cout << "twsearch serving http://127.0.0.1:" << serveport << "/ from "
       << (echosearches ? abbreviatehome(selfpath) : selfpath) << endl;
  /*
   *   Where the searches leave their pruning tables.  A search says so
   *   itself as it writes or reads one, but with the reader's home
   *   directory left out (that output goes to a page), so say it in full
   *   here, where only the person who started this is reading.  Unless
   *   this stream is the transcript, which travels the same way.
   */
  {
    string cachedir = prune_table_dir(false);
    cout << "pruning tables in "
         << (echosearches ? abbreviatehome(cachedir) : cachedir) << endl;
  }
  cout << flush;
  bool ok = server.listen_after_bind();
  filesystem::remove_all(workdir);
  if (!ok)
    error("! stopped serving unexpectedly");
  return 0;
}

static struct servecmd : specialopt {
  servecmd()
      : specialopt("--serve",
                   "Serve searches over HTTP to a web page on this machine\n"
                   "(see --port and --allow-origin).  The page does the\n"
                   "asking; this does the searching.") {
    // Every twsearch does this, not just the server: a search started by a
    // server exits when that server does.
    parentwatchhook = watchparent;
  }
  virtual void parse_args(int *, const char ***) { servehook = runserver; }
} registerserve;

static struct appcmd : cmd {
  appcmd()
      : cmd("--app",
            "url  Where --serve fetches the page it answers with; the\n"
            "default is https://cube20.org/gyrelab/ .  It wants a boot.js\n"
            "in it.") {}
  virtual void parse_args(int *argc, const char ***argv) {
    (*argc)--;
    (*argv)++;
    appurl = **argv;
    if (appurl.size() && appurl.back() != '/')
      appurl += '/';
  }
  virtual void docommand(puzdef &) { error("! bad docommand"); }
  virtual int ismaincmd() { return 0; }
} registerapp;

static struct noappcmd : cmd {
  noappcmd()
      : cmd("--no-app",
            "Serve searches only, and no page to ask for them with.") {}
  virtual void parse_args(int *, const char ***) { appurl.clear(); }
  virtual void docommand(puzdef &) { error("! bad docommand"); }
  virtual int ismaincmd() { return 0; }
} registernoapp;

static struct echocmd : cmd {
  echocmd()
      : cmd("--echo",
            "Write the puzzle, the scrambles, and the solver's output to\n"
            "standard output as they cross the bridge.  A session with one\n"
            "puzzle in it is a .tws file that runs the same searches again.") {}
  virtual void parse_args(int *, const char ***) { echosearches = 1; }
  virtual void docommand(puzdef &) { error("! bad docommand"); }
  virtual int ismaincmd() { return 0; }
} registerecho;

static struct portcmd : intopt {
  portcmd()
      : intopt("--port",
               "num  Port for --serve to listen on; the default is 2023.",
               &serveport, 1, 65535) {}
} registerport;

static struct alloworigincmd : cmd {
  alloworigincmd()
      : cmd("--allow-origin",
            "url  Let a page from this site use --serve.  Pages from this\n"
            "machine are always allowed; give this once per other site.\n"
            "The name null means a page opened from a file, which a page in\n"
            "a sandboxed iframe on any site also calls itself: allowing it\n"
            "lets any site on the web reach this server.") {}
  virtual void parse_args(int *argc, const char ***argv) {
    (*argc)--;
    (*argv)++;
    alloworigins.push_back(**argv);
  }
  virtual void docommand(puzdef &) { error("! bad docommand"); }
  virtual int ismaincmd() { return 0; }
} registeralloworigin;
