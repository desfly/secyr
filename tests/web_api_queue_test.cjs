const fs = require("fs");
const vm = require("vm");
const assert = require("assert");
const source = fs.readFileSync("web/app.js", "utf8");
const queueSource = source.slice(0, source.indexOf("const toast ="));
const calls = [];
let releaseFirst;
let running = 0;
let maxRunning = 0;
const context = {
  window: { fetch: async (url, init) => {
    calls.push(url);
    ++running;
    maxRunning = Math.max(maxRunning, running);
    try {
      if (url === "/api/v1/cloud/status") await new Promise(resolve => { releaseFirst = resolve; });
      if (url === "/api/v1/failing-read") throw new Error("test failure");
      return { ok: true, method: init.method || "GET" };
    } finally { --running; }
  } }, AbortController, Date, console, setTimeout, clearTimeout
};
vm.createContext(context);
vm.runInContext(queueSource, context);
(async () => {
  const first = context.window.fetch("/api/v1/cloud/status");
  const read = context.window.fetch("/api/v1/system/zones");
  const arm = context.window.fetch("/api/v1/system/security-command", { method: "POST" });
  const disarm = context.window.fetch("/api/v1/system/security-command-disarm", { method: "POST" });
  assert.deepStrictEqual(calls, ["/api/v1/cloud/status"]);
  releaseFirst();
  await Promise.all([first, read, arm, disarm]);
  assert.deepStrictEqual(calls, ["/api/v1/cloud/status", "/api/v1/system/security-command", "/api/v1/system/security-command-disarm", "/api/v1/system/zones"]);
  assert.strictEqual(maxRunning, 1);
  const failed = context.window.fetch("/api/v1/failing-read").catch(error => error.message);
  const after = context.window.fetch("/api/v1/system/partitions");
  assert.strictEqual(await failed, "test failure");
  assert.strictEqual((await after).ok, true);
  console.log("Web API queue: command priority, mutation FIFO, serialization and failure recovery PASS");
})().catch(error => { console.error(error); process.exitCode = 1; });
