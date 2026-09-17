/*
 *   A Web Worker running the WebAssembly twsearch (see twsearch-session.mjs
 *   for the behavior, which matches twsearch-bridge.mjs).
 *
 *   Messages to the worker:
 *      {type: "solve", id, tws, args, scramble}
 *      {type: "cancel", id}          (id optional: the running solve)
 *   Messages from the worker:
 *      {id, event}   where event is {type: "out"|"err", text},
 *                    {type: "done"} or {type: "error", message}
 *
 *   `make build-wasm` places this file next to twsearch.mjs.
 */
import createTwsearchModule from "./twsearch.mjs";
import { TwsearchSession } from "./twsearch-session.mjs";

const session = new TwsearchSession(createTwsearchModule, (id, event) =>
  self.postMessage({ id, event }),
);

self.addEventListener("message", (e) => {
  const msg = e.data;
  if (msg?.type === "solve") session.solve(msg);
  else if (msg?.type === "cancel") session.cancel(msg.id);
});
