// The protocol a page uses to have this twsearch do its searching: what
// `twsearch --serve` answers, and what src/js/twsearch-bridge.mjs answers
// under node.  Both must behave the same, since a page cannot tell them
// apart.
//
//    node test/bridge-test.mjs build/bin/twsearch   start that and test it
//    node test/bridge-test.mjs                      test whatever is already
//                                                   listening (BRIDGE_URL,
//                                                   or port 2023)
import { spawn, spawnSync } from "node:child_process";
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";

const startme = process.argv[2];
const port = Number(process.env.BRIDGE_PORT ?? (startme ? 2033 : 2023));
const base = process.env.BRIDGE_URL ?? `http://127.0.0.1:${port}`;
const origin = "http://localhost:3334";
let failures = 0;
let server = null;

const check = (ok, label, detail = "") => {
  console.log(`${ok ? "ok  " : "FAIL"} ${label}${detail ? " -- " + detail : ""}`);
  if (!ok) failures++;
};

// Start the program under test, and wait for it to answer.
if (startme) {
  // Started as it would really be run, with its own default memory cap; the
  // searches below ask for little, which is what keeps this quick.
  server = spawn(startme, ["--serve", "--port", String(port)], {
    stdio: ["ignore", "pipe", "pipe"],
  });
  server.stdout.on("data", (d) => process.stdout.write(`  [serve] ${d}`));
  server.stderr.on("data", (d) => process.stderr.write(`  [serve] ${d}`));
  server.on("exit", (code) => {
    if (code !== null && !server.killed) {
      console.log(`FAIL the server exited on its own (code ${code})`);
      process.exit(1);
    }
  });
  const until = Date.now() + 30000;
  for (;;) {
    try {
      await fetch(`${base}/v1/info`, { headers: { Origin: origin } });
      break;
    } catch {
      if (Date.now() > until) {
        console.log("FAIL the server never answered");
        server.kill();
        process.exit(1);
      }
      await new Promise((r) => setTimeout(r, 200));
    }
  }
}

const stop = () => server?.kill();

const read = (path) => readFileSync(new URL(`../${path}`, import.meta.url), "utf8");
const tws = read("samples/main/3x3x3.tws");
const tws222 = read("samples/main/2x2x2.tws");
const scramble = "ScrambleAlg alg\nR U R' F2\nEnd\n";

async function solve(body, { collect = true } = {}) {
  const res = await fetch(`${base}/v1/solve`, {
    method: "POST",
    headers: { "Content-Type": "application/json", Origin: origin },
    body: JSON.stringify(body),
  });
  if (!res.ok) return { status: res.status, text: (await res.text()).trim() };
  const events = [];
  const decoder = new TextDecoder();
  let buffer = "";
  for await (const chunk of res.body) {
    buffer += decoder.decode(chunk, { stream: true });
    const lines = buffer.split("\n");
    buffer = lines.pop();
    for (const line of lines) if (line.trim()) events.push(JSON.parse(line));
    if (!collect) break;
  }
  return { status: res.status, events };
}

const info = await (await fetch(`${base}/v1/info`, { headers: { Origin: origin } })).json();
check(info.bridge === "twsearch-bridge" && info.protocol === 1, "info says what it is", JSON.stringify(info));

const first = await solve({ id: "a", tws, args: ["-v2", "--checkbeforesolve", "-M", "64"], scramble });
const out = first.events.filter((e) => e.type === "out").map((e) => e.text).join("");
check(first.events.at(-1)?.type === "done", "a solve ends with done", first.events.at(-1)?.message ?? "");
check(/^ F2 R U' R'$/m.test(out), "the solution comes back", (out.match(/^ .*$/m) ?? [""])[0].trim());
check(out.includes("Solving"), "output streams, not just the answer");

// A different puzzle: the bridge stops the old twsearch and starts another.
const second = await solve({ id: "b", tws: tws222, args: ["-v2", "--checkbeforesolve", "-M", "64"], scramble: "ScrambleAlg alg\nR U\nEnd\n" });
check(second.events.at(-1)?.type === "done", "a second puzzle solves", second.events.at(-1)?.message ?? "");

// And back again, which must not be confused by the first puzzle's state.
const third = await solve({ id: "c", tws, args: ["-v2", "--checkbeforesolve", "-M", "64"], scramble });
check(third.events.at(-1)?.type === "done", "back to the first puzzle", third.events.at(-1)?.message ?? "");

// Options that are not allowed must be refused before anything runs.
const refused = await solve({ id: "d", tws, args: ["--cachedir", "/tmp"], scramble });
check(refused.status === 400, "an option that is not allowed is refused", `status ${refused.status} ${refused.text ?? ""}`);

// Cancel: start a long search, then ask it to stop.
const hard = "ScrambleAlg alg\nU R2 F B R B2 R U2 L B2 R U' D' R2 F R' L B2 U2 F2\nEnd\n";
const pending = solve({ id: "e", tws, args: ["-v2", "--checkbeforesolve", "-M", "64"], scramble: hard });
await new Promise((r) => setTimeout(r, 4000));
await fetch(`${base}/v1/cancel`, {
  method: "POST",
  headers: { "Content-Type": "application/json", Origin: origin },
  body: JSON.stringify({ id: "e" }),
});
const canceled = await pending;
const canceledText = canceled.events.filter((e) => e.type === "out").map((e) => e.text).join("");
check(/Search canceled at depth/.test(canceledText), "a search can be canceled", (canceledText.match(/Search canceled.*/) ?? [""])[0]);
check(canceled.events.at(-1)?.type === "done", "and the solve then ends");

// Still usable afterwards.
const after = await solve({ id: "f", tws, args: ["-v2", "--checkbeforesolve", "-M", "64"], scramble });
check(after.events.at(-1)?.type === "done", "still works after a cancel");

// --echo: a transcript of what crossed the bridge, on standard output.  The
// solver's output appears as the page saw it, and the puzzle and scramble
// blocks appear as they are, so they can be lifted out and run again.  Its
// own server, so the transcript holds one puzzle and one search.
if (startme) {
  const echoport = port + 1;
  const echoserver = spawn(startme, ["--serve", "--port", String(echoport), "--no-app", "--echo"], {
    stdio: ["ignore", "pipe", "pipe"],
  });
  let transcript = "";
  let echoerrors = "";
  echoserver.stdout.on("data", (d) => { transcript += d; });
  // Drain this too: nobody reading it is a pipe that fills and a server
  // that stops.
  echoserver.stderr.on("data", (d) => { echoerrors += d; });
  const echobase = `http://127.0.0.1:${echoport}`;
  const until = Date.now() + 30000;
  let answered = false;
  for (;;) {
    try {
      await fetch(`${echobase}/v1/info`, { headers: { Origin: origin } });
      answered = true;
      break;
    } catch {
      if (Date.now() > until) break;
      await new Promise((r) => setTimeout(r, 200));
    }
  }
  check(answered, "echo: the server answers");
  const stream = await fetch(`${echobase}/v1/solve`, {
    method: "POST",
    headers: { "Content-Type": "application/json", Origin: origin },
    body: JSON.stringify({ id: "echo", tws, args: ["-v2", "-M", "64"], scramble }),
  }).then((r) => r.text());
  echoserver.kill();
  await new Promise((r) => setTimeout(r, 300));

  // What the page was sent, in the pieces it was sent in.  The transcript
  // holds exactly that, so a half written line reaches both at once.
  const sent = stream
    .split("\n")
    .filter((l) => l)
    .map((l) => JSON.parse(l))
    .filter((e) => e.type === "out")
    .map((e) => e.text)
    .join("");
  if (!transcript.includes(sent)) {
    // Nothing else here can be understood without seeing what did arrive.
    console.log(`  [echo] transcript (${transcript.length} bytes): ${JSON.stringify(transcript)}`);
    console.log(`  [echo] stderr: ${JSON.stringify(echoerrors.slice(0, 400))}`);
    console.log(`  [echo] wanted (${sent.length} bytes): ${JSON.stringify(sent)}`);
  }
  check(transcript.includes(sent), "echo: the transcript is what the page was sent, byte for byte");
  check(transcript.includes(" F2 R U' R'"), "echo: the solution appears as the page saw it");
  check(transcript.includes(tws.trim()), "echo: the puzzle appears as it arrived");
  check(transcript.includes(scramble.trim()), "echo: the scramble appears as it arrived");
  // The claim that those blocks can be lifted out and run.
  mkdirSync("build/test", { recursive: true });
  const lifted = "build/test/lifted.tws";
  writeFileSync(lifted, `${tws}\n${scramble}`);
  const rerun = spawnSync(startme, ["-M", "64", "--nowrite", lifted], { encoding: "utf8" });
  check(/^ F2 R U' R'$/m.test(rerun.stdout ?? ""), "echo: a lifted out block solves the same position");
}

stop();
console.log(failures === 0 ? "All bridge checks passed." : `${failures} failure(s).`);
process.exit(failures === 0 ? 0 : 1);
