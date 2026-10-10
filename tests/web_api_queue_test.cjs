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
      return new Response("{}", { status: 200 });
    } finally { --running; }
  } }, AbortController, Response, Date, console, setTimeout, clearTimeout
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
  let finishBody;
  let bodyStarted;
  const headersReady = new Promise(resolve => { bodyStarted = resolve; });
  // nativeFetch is captured by the production wrapper; replace the browser's
  // original implementation in a fresh context to model headers before body.
  const bodyCalls = [];
  const bodyContext = {
    window: { fetch: async (url, init) => {
      bodyCalls.push(url);
      if (url === "/api/v1/slow-body") {
        return new Response(new ReadableStream({ start(controller) {
          finishBody = () => { controller.enqueue(new TextEncoder().encode("{\"ok\":true}")); controller.close(); };
          bodyStarted();
        } }));
      }
      if (url === "/api/v1/stalled-body") {
        return new Response(new ReadableStream({ start(controller) {
          init.signal.addEventListener("abort", () => controller.error(new Error("body aborted")), { once: true });
        } }));
      }
      return new Response("{}");
    } }, AbortController, Response, Date, console, setTimeout, clearTimeout
  };
  vm.createContext(bodyContext);
  vm.runInContext(queueSource, bodyContext);
  const slow = bodyContext.window.fetch("/api/v1/slow-body");
  await headersReady;
  const queued = bodyContext.window.fetch("/api/v1/queued-command", { method: "POST" });
  await new Promise(resolve => setTimeout(resolve, 10));
  assert.deepStrictEqual(bodyCalls, ["/api/v1/slow-body"]);
  finishBody();
  assert.strictEqual(await (await slow).text(), '{"ok":true}');
  await queued;
  bodyContext.setTimeout = (callback) => setTimeout(callback, 25);
  const stalled = bodyContext.window.fetch("/api/v1/stalled-body").catch(error => error.message);
  const recovered = bodyContext.window.fetch("/api/v1/recovered");
  assert.strictEqual(await stalled, "body aborted");
  assert.strictEqual((await recovered).ok, true);
  assert.deepStrictEqual(bodyCalls.slice(-2), ["/api/v1/stalled-body", "/api/v1/recovered"]);
  console.log("Web API queue: command priority, mutation FIFO, serialization and failure recovery PASS");
})().catch(error => { console.error(error); process.exitCode = 1; });
