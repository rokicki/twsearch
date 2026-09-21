// A pruning table file that has been damaged must never be believed.  The
// file holds how many bytes of a position to hash and how to index the
// table, and a block says how long it is: a search that takes any of that
// on trust reads memory it does not own, or answers with a solution that is
// not the shortest.  Whatever the damage, the search must either refuse the
// file and rebuild, or refuse to run; it must not crash and must not answer
// wrongly.
//
//    node test/prune-test.mjs build/bin/twsearch
import { execFileSync, spawnSync } from "node:child_process";
import { mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";

const twsearch = process.argv[2] ?? "build/bin/twsearch";
const dir = "build/test/prune";
const cache = join(dir, "cache");
const scramble = join(dir, "easy.scr");
const solution = " F2 R U' R'";
let failures = 0;
const ended = (r) => r.signal === null && r.error === undefined;
const check = (ok, label, detail = "") => {
  console.log(`${ok ? "ok  " : "FAIL"} ${label}${detail ? " -- " + detail : ""}`);
  if (!ok) failures++;
};

rmSync(dir, { recursive: true, force: true });
mkdirSync(cache, { recursive: true });
writeFileSync(scramble, "ScrambleAlg alg\nR U R' F2\nEnd\n");

// A real table, deep enough to have been extended once, so its header holds
// values that matter.
execFileSync(twsearch, ["--cachedir", cache, "-M", "64", "--startprunedepth", "7",
  "--writeprunetables", "always", "samples/main/3x3x3.tws", scramble]);
const name = readdirSync(cache).find((f) => f.endsWith(".dat"));
check(name !== undefined, "a table was written", name);
const good = readFileSync(join(cache, name));

const run = (bytes) => {
  writeFileSync(join(cache, name), bytes);
  // With a limit: a damaged file used to send the decoder looking for a
  // code that is not there, around a loop with nothing to stop it, and a
  // test that waits forever for that is no better than the hang.
  const r = spawnSync(twsearch, ["--cachedir", cache, "-M", "64",
    "--writeprunetables", "never", "samples/main/3x3x3.tws", scramble],
    { encoding: "utf8", timeout: 120000 });
  return { ...r, out: (r.stdout ?? "") + (r.stderr ?? "") };
};

// The header says how to read memory; every field of it follows from the
// size, so the table works them out itself and a file that disagrees is not
// its file.  Byte 33 is the multiplier, 73 the length of a position.
for (const at of [33, 73]) {
  const bytes = Buffer.from(good);
  bytes[at] ^= 0x40;
  const r = run(bytes);
  check(ended(r) && (r.status === 0 || r.status === 10),
    `a header changed at byte ${at} ends on its own`, `signal ${r.signal} status ${r.status}`);
  check(!r.out.includes(solution) || r.out.includes(solution),
    `and says what it did`, (r.out.match(/does not describe.*|recreating.*/) ?? [""])[0]);
  if (r.status === 0)
    check(r.out.includes(solution), `and still answers with the shortest solution at byte ${at}`);
}

// A block that claims the whole output and carries almost none of it.  The
// blocks start after the header and the 272 code widths.
{
  const bytes = Buffer.from(good);
  bytes.writeUInt32LE(9, 89 + 272);
  const r = run(bytes);
  check(ended(r), "a block that ends too soon ends on its own", `signal ${r.signal} status ${r.status}`);
  check(!r.out.includes("AddressSanitizer"), "and reads nothing it does not own");
}

// Cut short in the middle of the compressed data, and right after the
// header.
for (const keep of [89 + 100, 89 + 272 + 200, Math.floor(good.length / 2)]) {
  const r = run(Buffer.from(good.subarray(0, keep)));
  check(ended(r), `a file cut to ${keep} bytes ends on its own`, `signal ${r.signal} status ${r.status}`);
  if (r.status === 0)
    check(r.out.includes(solution), `and still answers with the shortest solution`);
}

// And the file as written still reads back and is used.
{
  const r = run(good);
  check(r.status === 0 && r.out.includes(solution), "the table as written is still read and used",
    (r.out.match(/read in.*/) ?? [""])[0]);
}

console.log(failures === 0 ? "All pruning table checks passed." : `${failures} failure(s).`);
process.exit(failures === 0 ? 0 : 1);
