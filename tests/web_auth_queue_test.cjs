const fs = require('fs');
const vm = require('vm');
const assert = require('assert');
const app = fs.readFileSync('web/app.js', 'utf8');
const auth = fs.readFileSync('web/access-session.js', 'utf8');
const calls = [];
let release;
let testingRecovery = false;
let probeStatus = 200;
let recoveries = 0;
const context = {window: {fetch: async (url, init) => {
  calls.push({url, auth: new Headers(init?.headers).get('Authorization')});
  if (url.endsWith('/zones') && !testingRecovery) await new Promise(resolve => {release = resolve;});
  return new Response('{}', {status: testingRecovery ? (url.endsWith('/zones') ? probeStatus : 401) : 200});
}}, recoverAccessGate: async () => {++recoveries;}, Headers, Response, AbortController, Date, console, setTimeout, clearTimeout};
vm.createContext(context);
vm.runInContext(app.slice(0, app.indexOf('const toast =')), context);
const wrapper = auth.slice(auth.indexOf('  const bearerMutationRoutes'), auth.indexOf('  function syncActorFields'));
vm.runInContext(`(() => {
  const operationalFetch = window.fetch.bind(window);
  const originalFetch = window.__homeguardNativeFetch || operationalFetch;
  let session = {token:"test-token",actor:"admin"};
  let authRecoveryPromise = null;
  function authHeader() { return "Bearer " + session.token; }
  ${wrapper}
})();`, context);
(async () => {
  const first = context.window.fetch('/api/v1/system/zones');
  const second = context.window.fetch('/api/v1/system/outputs');
  assert.strictEqual(calls.length, 1, 'authenticated requests must use operational queue');
  assert.strictEqual(calls[0].auth, 'Bearer test-token');
  release();
  await Promise.all([first, second]);
  assert.strictEqual(calls.length, 2);
  assert.strictEqual(calls[1].auth, 'Bearer test-token');
  testingRecovery = true;
  await context.window.fetch('/api/v1/hardware/analog');
  await new Promise(resolve => setTimeout(resolve, 20));
  assert.strictEqual(recoveries, 0, 'successful protected probe preserves session');
  assert.strictEqual(calls.at(-1).url, '/api/v1/system/zones');
  assert.strictEqual(calls.at(-1).auth, 'Bearer test-token');
  probeStatus = 401;
  await context.window.fetch('/api/v1/hardware/analog');
  await new Promise(resolve => setTimeout(resolve, 20));
  assert.strictEqual(recoveries, 1, 'expired bearer opens login after protected 401');
  assert.ok(!calls.some(call => call.url === '/api/v1/access/state'));
  console.log('Authenticated fetch: queue, bearer and protected session verification PASS');
})().catch(error => { console.error(error); process.exitCode = 1; });
