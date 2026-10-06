const fs = require('fs');
const vm = require('vm');
const assert = require('assert');
const app = fs.readFileSync('web/app.js', 'utf8');
const auth = fs.readFileSync('web/access-session.js', 'utf8');
const calls = [];
let release;
const context = {window: {fetch: async (url, init) => {
  calls.push({url, auth: new Headers(init?.headers).get('Authorization')});
  if (url.endsWith('/zones')) await new Promise(resolve => {release = resolve;});
  return new Response('{}');
}}, Headers, Response, AbortController, Date, console, setTimeout, clearTimeout};
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
  console.log('Authenticated fetch preserves queue and bearer header PASS');
})().catch(error => { console.error(error); process.exitCode = 1; });
