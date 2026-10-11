const fs = require('fs');
const vm = require('vm');
const assert = require('assert');
const source = fs.readFileSync('web/dashboard-live.js', 'utf8');
function extract(name, next) {return source.slice(source.indexOf('  '+name), source.indexOf('  '+next, source.indexOf('  '+name)));}
let release;
let calls = 0;
let rendered;
const context = {window: {HomeGuardAuth: {authenticated: () => true}}, live: {},
 api: () => {++calls; return new Promise(resolve => {release = resolve;});},
 syncOutputSnapshot: () => {}, renderLiveZones: (states, mv) => {rendered = {states, mv};}};
vm.createContext(context);
vm.runInContext(extract('function zoneStateFromMv','function renderLiveZones') +
 extract('async function refreshOutputs','async function refreshAnalogZones') +
 extract('async function refreshAnalogZones','async function commandOutput'), context);
(async () => {
 const first = context.refreshAnalogZones();
 await context.refreshAnalogZones();
 assert.strictEqual(calls, 1, 'polling must not overlap an unfinished read');
 release({devices: [{role:'zones', channels_mv:[null, 1600, 0, 3300]},
 {role:'telemetry',channels_mv:[1600,null,0,3300]}]});
 await first;
 assert.deepStrictEqual(Array.from(rendered.states), [3,0,4,1,0,3,4,1]);
 assert.strictEqual(rendered.mv[0], null);
 assert.strictEqual(rendered.mv[4], 1600, 'second ADS supplies zone 5');
 assert.strictEqual(rendered.mv[5], null, 'invalid zone 6 stays unavailable');
 const next = context.refreshAnalogZones();
 assert.strictEqual(calls, 2, 'polling must resume after completion');
 release({devices:[]}); await next;
 console.log('Dashboard polling: single-flight, invalid ADC and both zone modules PASS');
})().catch(error => {console.error(error);process.exitCode=1;});
