/*
 *   Runs the WebAssembly twsearch with the same behavior as the native
 *   bridge (twsearch-bridge.mjs), so a client can use either one the same
 *   way:
 *
 *   - solve({id, tws, args, scramble}) queues a solve.  Its progress arrives
 *     as events passed to emit(id, event), where event is one of
 *        {type: "out", text}      text twsearch wrote to stdout
 *        {type: "err", text}      text twsearch wrote to stderr
 *                                 (as flushed, so possibly a partial line)
 *        {type: "done"}           the solve finished (solved, no solution,
 *                                 or "Search canceled at depth d")
 *        {type: "error", message} the solve failed
 *   - cancel(id) cancels a solve: a queued one is dropped (with an error
 *     event), and a running one prints "Search canceled at depth d" and
 *     finishes with "done".  With no id, it cancels the running solve.
 *   - One puzzle is kept at a time.  A solve with a different puzzle or
 *     argument list replaces the instance, failing solves of the old one;
 *     the pruning table is otherwise reused across solves.
 *   - After an error the instance is discarded, as the native process
 *     would have exited, and the next solve starts afresh.
 */
export class TwsearchSession {
  #createModule;
  #emit;
  #queue = [];
  #current = null; // the job being solved
  #instance = null; // {key, module, dead}
  #pumping = false;

  /**
   * @param createModule the default export of twsearch.mjs
   * @param emit (id, event) => void
   */
  constructor(createModule, emit) {
    this.#createModule = createModule;
    this.#emit = emit;
  }

  solve({ id, tws, args = [], scramble }) {
    const job = { id, tws, args, scramble, key: JSON.stringify([args, tws]) };
    if (this.#current && this.#current.key !== job.key)
      this.#retire("puzzle changed");
    this.#queue = this.#queue.filter((queued) => {
      if (queued.key === job.key) return true;
      this.#emit(queued.id, { type: "error", message: "puzzle changed" });
      return false;
    });
    this.#queue.push(job);
    void this.#pump();
  }

  cancel(id) {
    const i = this.#queue.findIndex((job) => job.id === id);
    if (id !== undefined && i >= 0) {
      const [job] = this.#queue.splice(i, 1);
      this.#emit(job.id, {
        type: "error",
        message: "canceled before it started",
      });
    } else if (
      this.#current &&
      (id === undefined || id === this.#current.id) &&
      this.#instance
    ) {
      this.#instance.module.twsearchCanceled = true;
    }
  }

  /** Stop using the current instance; its running solve ends with message. */
  #retire(message) {
    const instance = this.#instance;
    if (!instance) return;
    instance.dead = message;
    // Stop its search promptly; its output from here on is dropped.
    instance.module.twsearchCanceled = true;
    this.#instance = null;
  }

  async #pump() {
    if (this.#pumping) return;
    this.#pumping = true;
    while (this.#queue.length) {
      const job = this.#queue.shift();
      job.stderr = "";
      this.#current = job;
      let instance = this.#instance;
      try {
        if (!instance || instance.key !== job.key) {
          instance = { key: job.key, module: null, dead: null };
          instance.module = await this.#createModule({
            twsearchOutput: (fd, text) => {
              if (instance.dead || !this.#current) return;
              const type = fd === 2 ? "err" : "out";
              if (type === "err") this.#current.stderr += text;
              this.#emit(this.#current.id, { type, text });
            },
          });
          this.#instance = instance;
          instance.module.ccall("w_args", null, ["string"], [job.args.join(" ")]);
          instance.module.ccall("w_setksolve", null, ["string"], [job.tws]);
        }
        await instance.module.ccall(
          "w_solvescramble",
          null,
          ["string"],
          [job.scramble],
          { async: true },
        );
        if (instance.dead) throw new Error(instance.dead);
        this.#emit(job.id, { type: "done" });
      } catch (e) {
        if (this.#instance === instance) this.#instance = null;
        // Same message as the bridge: why we dropped the instance, else
        // what twsearch said on stderr.
        this.#emit(job.id, {
          type: "error",
          message:
            instance.dead ??
            (job.stderr.trim()
              ? job.stderr.trim().split("\n").slice(-20).join(" / ")
              : (e?.message ?? String(e))),
        });
      }
      this.#current = null;
    }
    this.#pumping = false;
  }
}
