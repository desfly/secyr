"use strict";

(() => {
  let currentConfig = null;

  const q = (s) => document.querySelector(s);
  const actor = () => q("#operatorId")?.value?.trim() || "";
  const credential = () => q("#operatorPin")?.value || "";
  const authHeaders = () => ({ "X-HG-Actor": actor(), "X-HG-Credential": credential() });

  function ensureUi() {
    const host = q("#system");
    if (!host || q("#configManager")) return;
    const panel = document.createElement("article");
    panel.id = "configManager";
    panel.className = "panel";
    panel.style.cssText = "margin-top:18px;max-width:1200px";
    panel.innerHTML = `
      <h3>Конфігурація HomeGuard-S3</h3>
      <p>Зони, виходи та права 8 користувачів. PIN/секрети не експортуються.</p>
      <div style="display:flex;gap:8px;flex-wrap:wrap;margin:12px 0">
        <button id="configLoad" type="button">Завантажити поточну</button>
        <button id="configExport" type="button">Експорт JSON</button>
        <label style="display:inline-flex;align-items:center;gap:8px;border:1px solid #d7deea;padding:8px 12px;border-radius:8px;cursor:pointer">Імпорт JSON<input id="configFile" type="file" accept="application/json,.json" hidden></label>
        <button id="configApply" type="button">Застосувати зміни</button>
        <span id="configState">—</span>
      </div>
      <h4>Зони</h4><div id="configZones" style="overflow:auto"></div>
      <h4 style="margin-top:18px">Виходи</h4><div id="configOutputs" style="overflow:auto"></div>
      <details style="margin-top:18px"><summary>JSON для ручного редагування</summary>
        <textarea id="configJson" spellcheck="false" style="width:100%;min-height:320px;margin-top:10px;font:12px/1.45 monospace"></textarea>
      </details>`;
    host.appendChild(panel);
    q("#configLoad")?.addEventListener("click", loadConfig);
    q("#configExport")?.addEventListener("click", exportConfig);
    q("#configApply")?.addEventListener("click", applyEditor);
    q("#configFile")?.addEventListener("change", importFile);
  }

  function state(text, bad = false) {
    const node = q("#configState"); if (!node) return; node.textContent = text; node.style.fontWeight = "700"; node.style.color = bad ? "#b42318" : "#147a44";
  }

  async function fetchConfig() {
    if (!actor() || !credential()) throw new Error("Введіть Admin ID та PIN");
    const r = await fetch("/api/v1/config/export", { cache: "no-store", headers: authHeaders() });
    if (!r.ok) throw new Error(`Export HTTP ${r.status}: ${await r.text()}`);
    return r.json();
  }

  function userIds(config) { return Array.isArray(config.users) ? config.users.map((u) => u.id).slice(0, 8) : []; }
  function zoneEntry(config, userId) { return (config.zoneAccess || []).find((x) => x.userId === userId); }
  function outputEntry(config, userId) { return (config.outputAccess || []).find((x) => x.userId === userId); }

  function check(label, checked, onChange) {
    const wrap = document.createElement("label"); wrap.style.cssText = "display:inline-flex;gap:3px;align-items:center;margin-right:6px;white-space:nowrap;font-size:11px";
    const input = document.createElement("input"); input.type = "checkbox"; input.checked = !!checked; input.addEventListener("change", () => onChange(input.checked));
    wrap.append(input, document.createTextNode(label)); return wrap;
  }

  function tableShell(headers) {
    const t = document.createElement("table"); t.style.cssText = "border-collapse:collapse;width:100%;font-size:12px";
    const tr = document.createElement("tr"); headers.forEach((h) => { const th=document.createElement("th"); th.textContent=h; th.style.cssText="text-align:left;border-bottom:1px solid #d7deea;padding:7px;white-space:nowrap"; tr.appendChild(th); });
    const head=document.createElement("thead"); head.appendChild(tr); t.appendChild(head); t.appendChild(document.createElement("tbody")); return t;
  }

  function renderZones(config) {
    const host=q("#configZones"); if(!host) return; host.textContent="";
    const users=userIds(config); const table=tableShell(["ID","Назва","Тип","Вхід, с","Вихід, с",...users]); const body=table.tBodies[0];
    (config.zones||[]).forEach((zone) => {
      const tr=document.createElement("tr");
      const fixed=[zone.id,zone.name,zone.type,zone.entryDelaySec,zone.exitDelaySec];
      fixed.forEach((v,idx)=>{const td=document.createElement("td");td.style.cssText="padding:6px;border-bottom:1px solid #edf0f4";
        if(idx===1){const i=document.createElement("input");i.value=v??"";i.maxLength=23;i.addEventListener("change",()=>{zone.name=i.value;syncEditor();});td.appendChild(i);}
        else if(idx===2){const s=document.createElement("select");["entry_exit","perimeter","interior","instant","fire_24h","flood_24h","tamper_24h","panic_24h"].forEach(x=>{const o=document.createElement("option");o.value=x;o.textContent=x;o.selected=x===v;s.appendChild(o)});s.addEventListener("change",()=>{zone.type=s.value;syncEditor();});td.appendChild(s);}
        else if(idx===3||idx===4){const i=document.createElement("input");i.type="number";i.min="0";i.max="3600";i.value=v??0;i.style.width="70px";i.addEventListener("change",()=>{if(idx===3)zone.entryDelaySec=Number(i.value);else zone.exitDelaySec=Number(i.value);syncEditor();});td.appendChild(i);}
        else td.textContent=String(v); tr.appendChild(td);});
      users.forEach((uid)=>{const td=document.createElement("td");td.style.cssText="padding:6px;border-bottom:1px solid #edf0f4;min-width:150px";let e=zoneEntry(config,uid);if(!e){e={userId:uid,zones:[]};config.zoneAccess.push(e)}let r=e.zones.find(x=>x.id===zone.id);if(!r){r={id:zone.id,view:true,arm:false,disarm:false,bypass:false};e.zones.push(r)};
        td.append(check("V",r.view,v=>{r.view=v;syncEditor()}),check("A",r.arm,v=>{r.arm=v;syncEditor()}),check("D",r.disarm,v=>{r.disarm=v;syncEditor()}),check("B",r.bypass,v=>{r.bypass=v;syncEditor()}));tr.appendChild(td);});
      body.appendChild(tr);
    }); host.appendChild(table);
  }

  function renderOutputs(config) {
    const host=q("#configOutputs"); if(!host)return; host.textContent=""; const users=userIds(config); const table=tableShell(["ID","Назва","Тип","Timeout, с",...users]); const body=table.tBodies[0];
    (config.outputs||[]).forEach((output)=>{const tr=document.createElement("tr");[output.id,output.name,output.type,output.timeoutSec].forEach((v,idx)=>{const td=document.createElement("td");td.style.cssText="padding:6px;border-bottom:1px solid #edf0f4";
      if(idx===1){const i=document.createElement("input");i.value=v??"";i.maxLength=23;i.addEventListener("change",()=>{output.name=i.value;syncEditor()});td.appendChild(i)}
      else if(idx===2){const s=document.createElement("select");["relay","siren","valve","light"].forEach(x=>{const o=document.createElement("option");o.value=x;o.textContent=x;o.selected=x===v;s.appendChild(o)});s.addEventListener("change",()=>{output.type=s.value;syncEditor()});td.appendChild(s)}
      else if(idx===3){const i=document.createElement("input");i.type="number";i.min="0";i.max="86400";i.value=v??0;i.style.width="80px";i.addEventListener("change",()=>{output.timeoutSec=Number(i.value);syncEditor()});td.appendChild(i)} else td.textContent=String(v);tr.appendChild(td)});
      users.forEach((uid)=>{const td=document.createElement("td");td.style.cssText="padding:6px;border-bottom:1px solid #edf0f4;min-width:120px";let e=outputEntry(config,uid);if(!e){e={userId:uid,outputs:[]};config.outputAccess.push(e)}let r=e.outputs.find(x=>x.id===output.id);if(!r){r={id:output.id,view:true,on:false,off:false};e.outputs.push(r)};td.append(check("V",r.view,v=>{r.view=v;syncEditor()}),check("ON",r.on,v=>{r.on=v;syncEditor()}),check("OFF",r.off,v=>{r.off=v;syncEditor()}));tr.appendChild(td)});body.appendChild(tr)});host.appendChild(table);
  }

  function syncEditor(){const e=q("#configJson");if(e&&currentConfig)e.value=JSON.stringify(currentConfig,null,2)}
  function render(config){currentConfig=config;renderZones(config);renderOutputs(config);syncEditor()}

  async function loadConfig(){try{state("Завантаження…");render(await fetchConfig());state("Поточну конфігурацію завантажено")}catch(e){state(e.message,true)}}
  async function exportConfig(){try{state("Експорт…");const config=await fetchConfig();render(config);const blob=new Blob([JSON.stringify(config,null,2)+"\n"],{type:"application/json"});const a=document.createElement("a");a.href=URL.createObjectURL(blob);a.download="homeguard-s3-config.json";a.click();setTimeout(()=>URL.revokeObjectURL(a.href),1000);state("JSON експортовано")}catch(e){state(e.message,true)}}
  async function applyText(text){if(!actor()||!credential())throw new Error("Введіть Admin ID та PIN");JSON.parse(text);const r=await fetch("/api/v1/config/import",{method:"POST",headers:{...authHeaders(),"Content-Type":"application/json"},body:text});if(!r.ok)throw new Error(`Import HTTP ${r.status}: ${await r.text()}`);render(await fetchConfig());state("Конфігурацію перевірено, збережено і застосовано")}
  async function applyEditor(){try{state("Перевірка та застосування…");await applyText(q("#configJson")?.value||"")}catch(e){state(e.message,true)}}
  async function importFile(event){try{const file=event.target.files?.[0];if(!file)return;state("Читання файлу…");const text=await file.text();q("#configJson").value=text;currentConfig=JSON.parse(text);render(currentConfig);await applyText(text)}catch(e){state(e.message,true)}finally{event.target.value=""}}

  function boot(){ensureUi();document.querySelector('a[href="#system"]')?.addEventListener("click",()=>setTimeout(ensureUi,0));}
  if(document.readyState==="loading")document.addEventListener("DOMContentLoaded",boot);else boot();
  window.homeguardConfigUi={load:loadConfig};
})();
