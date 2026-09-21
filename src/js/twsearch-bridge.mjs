#!/usr/bin/env node
/*
 *   twsearch-bridge: lets a web page (such as the Twizzle Explorer) use a
 *   natively compiled twsearch on this machine.
 *
 *      node src/js/twsearch-bridge.mjs [--port 2023] [--twsearch path]
 *           [--max-mem MB] [--allow-origin URL ...]
 *
 *   It listens on 127.0.0.1 only.  The page POSTs to /v1/solve a JSON body
 *
 *      { "id": <optional; any JSON value naming this solve>,
 *        "tws": "<contents of a .tws file>",
 *        "args": ["-c", "3", ...],
 *        "scramble": "<contents of a scramble file>" }
 *
 *   and gets back newline-delimited JSON events as twsearch runs:
 *
 *      {"type":"out","text":"..."}       text twsearch wrote to stdout
 *      {"type":"err","text":"..."}       text twsearch wrote to stderr
 *      {"type":"done"}                   twsearch finished
 *      {"type":"error","message":"..."}  twsearch failed
 *
 *   Text arrives as twsearch writes it, not split into lines, so a partial
 *   line that twsearch flushes (such as "Filling depth 7 val 2" before a long
 *   table fill) is delivered at once.  The wasm build emits the same events,
 *   so clients parse both alike.
 *
 *   The bridge keeps a single twsearch process (run with --nowrite, reading
 *   scrambles from standard input), so the pruning table built for the first
 *   solve is reused by later solves.  The process is replaced only when a
 *   request brings a different puzzle or argument list.  Requests for the
 *   same puzzle wait their turn.
 *
 *   POST /v1/cancel with {"id": ...} cancels that solve (with no id, the
 *   running one).  A queued solve is dropped.  For a running solve the
 *   bridge writes "!cancel" to twsearch's standard input; twsearch prints
 *   "Search canceled at depth d" (received on the solve's stream like any
 *   other line, followed by "done") and keeps the process and its pruning
 *   table.  A pruning table fill in progress completes first; to abandon it
 *   instead, send {"id": ..., "abandon": true}, which kills the process.
 *   Closing a solve request early cancels it (without abandoning).
 *
 *   src/js/twsearch-session.mjs gives the WebAssembly build this same
 *   behavior.
 *
 *   Only a fixed set of search options is accepted, -M is capped at
 *   The protocol is documented in docs/bridgeprotocol.md, and tested by
 *   test/bridge-test.mjs; `twsearch --serve` speaks the same one without
 *   needing node.
 *
 *   --max-mem, and requests are refused from origins other than localhost
 *   and the sites listed in allowOrigins below unless --allow-origin names
 *   them.  A page served from anywhere else needs, for example:
 *
 *       node twsearch-bridge.mjs --allow-origin https://example.org
 *
 *   The page's own server cannot grant this: the browser asks the bridge,
 *   so the bridge is what decides.
 */
import { spawn } from "node:child_process";
import { existsSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { createServer } from "node:http";
import { cpus, tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));

const opts = {
  port: 2023,
  twsearch: resolve(here, "../../build/bin/twsearch"),
  maxMem: 4096,
  allowOrigins: [
    "https://alpha.twizzle.net",
    "https://experiments.cubing.net",
    "https://cube20.org",
    "https://www.cube20.org",
  ],
};
{
  const argv = process.argv.slice(2);
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    const next = () => {
      if (i + 1 >= argv.length) usage(`${a} needs a value`);
      return argv[++i];
    };
    if (a === "--port") opts.port = Number(next());
    else if (a === "--twsearch") opts.twsearch = resolve(next());
    else if (a === "--max-mem") opts.maxMem = Number(next());
    else if (a === "--allow-origin") opts.allowOrigins.push(next());
    else if (a === "-h" || a === "--help") usage();
    else usage(`unknown option ${a}`);
  }
}

function usage(msg) {
  if (msg) console.error(msg);
  console.error(
    "usage: twsearch-bridge.mjs [--port 2023] [--twsearch path] [--max-mem MB]\n" +
      "                          [--allow-origin URL ...]",
  );
  process.exit(msg ? 1 : 0);
}

if (!existsSync(opts.twsearch)) {
  console.error(
    `twsearch binary not found at ${opts.twsearch}; run \`make build\` or pass --twsearch`,
  );
  process.exit(1);
}

// Options a page may pass, with the number of values each takes.
const ALLOWED_ARGS = new Map([
  ["-c", 1],
  ["-M", 1],
  ["-t", 1],
  ["-R", 1],
  ["--moves", 1],
  ["--mindepth", 1],
  ["--maxdepth", 1],
  ["--startprunedepth", 1],
  ["--microthreads", 1],
  ["--newcanon", 1],
  ["--omit", 1],
  ["--omitperms", 1],
  ["--omitoris", 1],
  ["--orientationgroup", 1],
  ["--writeprunetables", 1],
  ["--nowrite", 0],
  ["-q", 0],
  ["--quiet", 0],
  ["--alloptimal", 0],
  ["--randomstart", 0],
  ["--checkbeforesolve", 0],
  ["--noearlysolutions", 0],
  ["--nosymmetry", 0],
  ["--nocorners", 0],
  ["--nocenters", 0],
  ["--noedges", 0],
  ["--noorientation", 0],
  ["--distinguishall", 0],
]);

function checkArgs(args) {
  if (!Array.isArray(args) || args.some((a) => typeof a !== "string"))
    throw new Error("args must be an array of strings");
  const out = ["--nowrite"];
  let sawMem = false;
  for (let i = 0; i < args.length; i++) {
    // -v takes its level attached: -v, -v0 ... -v9
    const arity = /^-v\d?$/.test(args[i]) ? 0 : ALLOWED_ARGS.get(args[i]);
    if (arity === undefined)
      throw new Error(`option not allowed through the bridge: ${args[i]}`);
    out.push(args[i]);
    if (arity === 1) {
      if (i + 1 >= args.length) throw new Error(`${args[i]} needs a value`);
      let v = args[++i];
      if (args[i - 1] === "-M") {
        sawMem = true;
        const mb = Number(v);
        if (!Number.isInteger(mb) || mb <= 0) throw new Error("bad -M value");
        v = String(Math.min(mb, opts.maxMem));
      }
      out.push(v);
    }
  }
  if (!sawMem) out.push("-M", String(opts.maxMem));
  return out;
}

const workdir = mkdtempSync(join(tmpdir(), "twsearch-bridge-"));
process.on("exit", () => {
  running?.kill();
  rmSync(workdir, { recursive: true, force: true });
});
process.on("SIGINT", () => process.exit(130));
process.on("SIGTERM", () => process.exit(143));

// Lines that end twsearch's output for one scramble (see solve.cpp).
const END_OF_SOLVE =
  /^(Found \d+ solutions? |No solution found in |Ignoring unsolvable position\.|Search canceled at depth )/;

/** The one live twsearch process, if any. */
let running = null;

/** A twsearch process for one puzzle and argument list. */
class TwsearchProcess {
  constructor(key, tws, args) {
    this.key = key;
    this.dead = false;
    this.current = null; // the job whose scramble is being solved
    this.queue = [];
    this.stderrTail = [];
    const name = (/^\s*Name\s+(\S+)/m.exec(tws)?.[1] ?? "puzzle").replace(
      /[^A-Za-z0-9_-]/g,
      "_",
    );
    const twsFile = join(workdir, `${name}.tws`);
    writeFileSync(twsFile, tws);
    // Naming ourselves says two things to the child: stop when this
    // process is gone, and keep the reader's home directory out of what it
    // prints, since that goes to a page.  twsearch --serve does the same.
    this.child = spawn(opts.twsearch, [...args, twsFile, "-"], {
      stdio: ["pipe", "pipe", "pipe"],
      env: { ...process.env, TWSEARCH_SERVER_PID: String(process.pid) },
    });
    this.child.stdin.on("error", () => {});
    chunks(
      this.child.stdout,
      (text) => this.current?.emit({ type: "out", text }),
      (line) => this.onStdoutLine(line),
    );
    chunks(
      this.child.stderr,
      (text) => this.current?.emit({ type: "err", text }),
      (line) => {
        this.stderrTail.push(line);
        if (this.stderrTail.length > 20) this.stderrTail.shift();
      },
    );
    let spawnError = null;
    this.child.on("error", (e) => {
      spawnError = `could not run twsearch: ${e.message}`;
    });
    // "close" rather than "exit", so all output is delivered first.
    // The message matches twsearch-session.mjs: why we killed it, else what
    // twsearch said on stderr.
    this.child.on("close", (code, signal) =>
      this.onClose(
        this.killReason ??
          spawnError ??
          (this.stderrTail.length
            ? this.stderrTail.join(" / ")
            : `twsearch ${signal ? `killed (${signal})` : `exited with code ${code}`}`),
      ),
    );
  }

  onStdoutLine(line) {
    // Output before the first scramble (table building) goes to that job.
    if (!this.current) return;
    if (END_OF_SOLVE.test(line)) {
      const job = this.current;
      this.current = null;
      job.finish(null);
      this.next();
    }
  }

  onClose(message) {
    this.dead = true;
    if (running === this) running = null;
    const jobs = this.current ? [this.current, ...this.queue] : this.queue;
    this.current = null;
    this.queue = [];
    for (const job of jobs) job.finish(message);
  }

  solve(id, scramble, emit, finish) {
    const job = { id, scramble, emit, finish };
    this.queue.push(job);
    this.next();
    return job;
  }

  /** Cancel a job: drop it if still queued, else stop its search. */
  cancel(job, abandon = false) {
    const i = this.queue.indexOf(job);
    if (i >= 0) {
      this.queue.splice(i, 1);
      job.finish("canceled before it started");
    } else if (this.current === job) {
      if (abandon) this.kill("abandoned");
      else this.child.stdin.write("!cancel\n");
    }
  }

  findJob(id) {
    if (id === undefined) return this.current;
    if (this.current?.id === id) return this.current;
    return this.queue.find((job) => job.id === id);
  }

  next() {
    if (this.dead || this.current || this.queue.length === 0) return;
    this.current = this.queue.shift();
    this.stderrTail = [];
    const text = this.current.scramble;
    this.child.stdin.write(text.endsWith("\n") ? text : `${text}\n`);
  }

  kill(reason) {
    this.killReason ??= reason;
    this.child.kill("SIGKILL");
  }
}

function getProcess(tws, args) {
  const key = JSON.stringify([args, tws]);
  if (running && !running.dead && running.key === key) return running;
  running?.kill("puzzle changed");
  running = new TwsearchProcess(key, tws, args);
  return running;
}

/** Passes along each chunk as it arrives, then each line it completes. */
function chunks(stream, onText, onLine) {
  let buf = "";
  stream.setEncoding("utf8");
  stream.on("data", (chunk) => {
    onText(chunk);
    buf += chunk;
    let nl = buf.indexOf("\n");
    while (nl >= 0) {
      onLine(buf.slice(0, nl).replace(/\r$/, ""));
      buf = buf.slice(nl + 1);
      nl = buf.indexOf("\n");
    }
  });
  stream.on("end", () => {
    if (buf) onLine(buf);
  });
}

function originAllowed(origin) {
  if (origin === undefined) return true; // not a browser
  if (opts.allowOrigins.includes(origin)) return true;
  try {
    const u = new URL(origin);
    return (
      (u.protocol === "http:" || u.protocol === "https:") &&
      (u.hostname === "localhost" ||
        u.hostname === "127.0.0.1" ||
        u.hostname === "[::1]" ||
        u.hostname.endsWith(".localhost"))
    );
  } catch {
    return false;
  }
}

function hostAllowed(host) {
  // Guards against DNS rebinding: the page must address us by a loopback name.
  const h = (host ?? "").replace(/:\d+$/, "");
  return h === "127.0.0.1" || h === "localhost" || h === "[::1]";
}

const MAX_BODY = 32 * 1024 * 1024;

const server = createServer((req, res) => {
  const origin = req.headers.origin;
  if (!hostAllowed(req.headers.host) || !originAllowed(origin)) {
    res.writeHead(403).end("forbidden\n");
    return;
  }
  if (origin) {
    res.setHeader("Access-Control-Allow-Origin", origin);
    res.setHeader("Vary", "Origin");
  }
  if (req.method === "OPTIONS") {
    res.writeHead(204, {
      "Access-Control-Allow-Methods": "GET, POST",
      "Access-Control-Allow-Headers": "Content-Type",
      // Chrome's Private Network Access preflight, for https pages.
      "Access-Control-Allow-Private-Network": "true",
      "Access-Control-Max-Age": "600",
    });
    res.end();
    return;
  }
  const url = new URL(req.url, "http://127.0.0.1");
  if (req.method === "GET" && url.pathname === "/v1/info") {
    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(
      JSON.stringify({
        bridge: "twsearch-bridge",
        protocol: 1,
        threads: cpus().length,
        maxMem: opts.maxMem,
      }),
    );
    return;
  }
  if (req.method === "POST" && url.pathname === "/v1/cancel") {
    readBody(req, res, (body) => {
      const job = running?.findJob(body?.id ?? undefined);
      if (job) running.cancel(job, body?.abandon === true);
      res.writeHead(204).end();
    }, true);
    return;
  }
  if (req.method === "POST" && url.pathname === "/v1/solve") {
    readBody(req, res, (body) => handleSolve(body, res));
    return;
  }
  res.writeHead(404).end("not found\n");
});

function readBody(req, res, onBody, allowEmpty = false) {
  const chunks = [];
  let size = 0;
  req.on("data", (c) => {
    size += c.length;
    if (size > MAX_BODY) {
      res.writeHead(413).end("request too large\n");
      req.destroy();
      return;
    }
    chunks.push(c);
  });
  req.on("end", () => {
    if (size > MAX_BODY) return;
    let body;
    try {
      const text = Buffer.concat(chunks).toString("utf8");
      body = allowEmpty && text.trim() === "" ? {} : JSON.parse(text);
    } catch {
      res.writeHead(400).end("body must be JSON\n");
      return;
    }
    onBody(body);
  });
}

function handleSolve(body, res) {
  let args;
  try {
    if (typeof body.tws !== "string" || typeof body.scramble !== "string")
      throw new Error("tws and scramble must be strings");
    args = checkArgs(body.args ?? []);
  } catch (e) {
    res.writeHead(400).end(`${e.message}\n`);
    return;
  }
  res.writeHead(200, {
    "Content-Type": "application/x-ndjson",
    "Cache-Control": "no-store",
  });
  let finished = false;
  const emit = (event) => {
    if (!finished) res.write(`${JSON.stringify(event)}\n`);
  };
  const proc = getProcess(body.tws, args);
  const job = proc.solve(body.id, body.scramble, emit, (message) => {
    emit(message === null ? { type: "done" } : { type: "error", message });
    finished = true;
    res.end();
  });
  res.on("close", () => {
    // The client went away mid-search: treat it as a cancel.
    if (!finished) proc.cancel(job);
  });
}

server.listen(opts.port, "127.0.0.1", () => {
  console.log(
    `twsearch-bridge listening on http://127.0.0.1:${opts.port}/ using ${opts.twsearch}`,
  );
});
