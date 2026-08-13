"use strict";

(function () {
  const $ = (s) => document.querySelector(s);
  const esc = (v) => String(v ?? "").replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/\"/g,"&quot;");
  const checked = (v) => v ? "checked" : "";

  function auth() {
    return { actor: $("#accessActor")?.value.trim() || "", credential: $("#accessCredential")?.value.trim() || "" };
  }
  async function req(path, options={}) {
    const r=await fetch(path,{cache:"no-store",...options,headers:{"Content-Type":"application/json",...(options.headers||{})}});
    const t=await r.text(); let b={}; try{b=t?JSON.parse(t):{};}catch(_){b={raw:t};}
    if(!r.ok||b.ok===false) throw new Error(b.reason||`${r.status} ${r.statusText}`); return b;
  }
  function ensurePanel(){
    if($("#accessMatrixPanel")) return $("#accessMatrixPanel");
    const host=$("#accessPanel"); if(!host) return null;
    const p=document.createElement("section"); p.id="accessMatrixPanel"; p.style.cssText="margin-top:20px;border-top:1px solid #e3e8ef;padding-top:16px";
    p.innerHTML=`<h3>Права зон і виходів</h3><p>Окремі дозволи для кожного користувача. Admin має повний доступ незалежно від таблиці.</p><div style="display:flex;gap:8px;align-items:end;flex-wrap:wrap"><label>Користувач<select id="matrixUser" style="display:block;margin-top:6px;padding:9px;min-width:180px"></select></label><button id="matrixLoad" type="button">Завантажити права</button><button id="matrixSave" type="button">Зберегти права</button><span id="matrixState">—</span></div><div id="matrixTables" style="margin-top:14px"></div>`;
    host.appendChild(p); $("#matrixLoad").onclick=load; $("#matrixSave").onclick=save; return p;
  }
  function render(data){
    const z=Array.isArray(data.zones)?data.zones:[], o=Array.isArray(data.outputs)?data.outputs:[];
    $("#matrixTables").innerHTML=`<h4>Зони</h4><div style="overflow:auto"><table style="width:100%;border-collapse:collapse"><thead><tr><th>ID</th><th>Перегляд</th><th>Поставити</th><th>Зняти</th><th>Обхід</th></tr></thead><tbody>${z.map(x=>`<tr data-z="${Number(x.id)}"><td>${Number(x.id)}</td><td><input data-k="view" type="checkbox" ${checked(x.view)}></td><td><input data-k="arm" type="checkbox" ${checked(x.arm)}></td><td><input data-k="disarm" type="checkbox" ${checked(x.disarm)}></td><td><input data-k="bypass" type="checkbox" ${checked(x.bypass)}></td></tr>`).join("")}</tbody></table></div><h4>Виходи</h4><div style="overflow:auto"><table style="width:100%;border-collapse:collapse"><thead><tr><th>ID</th><th>Перегляд</th><th>ON</th><th>OFF</th></tr></thead><tbody>${o.map(x=>`<tr data-o="${Number(x.id)}"><td>${Number(x.id)}</td><td><input data-k="view" type="checkbox" ${checked(x.view)}></td><td><input data-k="on" type="checkbox" ${checked(x.on)}></td><td><input data-k="off" type="checkbox" ${checked(x.off)}></td></tr>`).join("")}</tbody></table></div>`;
  }
  async function users(){
    ensurePanel(); const a=auth(); if(!a.actor||!a.credential) return;
    try{const d=await req(`/api/v1/access/users?actor=${encodeURIComponent(a.actor)}&credential=${encodeURIComponent(a.credential)}`); const u=Array.isArray(d.users)?d.users:[]; $("#matrixUser").innerHTML=u.map(x=>`<option value="${esc(x.id)}">${esc(x.name||x.id)} (${esc(x.role)})</option>`).join("");}catch(e){$("#matrixState").textContent=`Помилка: ${e.message}`;}
  }
  async function load(){const a=auth(), user=$("#matrixUser")?.value||""; if(!a.actor||!a.credential||!user){$("#matrixState").textContent="Введіть Admin ID/PIN та виберіть користувача";return;} try{$("#matrixState").textContent="Завантаження…"; const d=await req(`/api/v1/access/matrix?actor=${encodeURIComponent(a.actor)}&credential=${encodeURIComponent(a.credential)}&userId=${encodeURIComponent(user)}`); render(d); $("#matrixState").textContent="Завантажено";}catch(e){$("#matrixState").textContent=`Помилка: ${e.message}`;}}
  async function save(){const a=auth(), user=$("#matrixUser")?.value||""; if(!a.actor||!a.credential||!user)return; const zones=[...document.querySelectorAll("[data-z]")].map(r=>({id:Number(r.dataset.z),view:r.querySelector('[data-k="view"]').checked,arm:r.querySelector('[data-k="arm"]').checked,disarm:r.querySelector('[data-k="disarm"]').checked,bypass:r.querySelector('[data-k="bypass"]').checked})); const outputs=[...document.querySelectorAll("[data-o]")].map(r=>({id:Number(r.dataset.o),view:r.querySelector('[data-k="view"]').checked,on:r.querySelector('[data-k="on"]').checked,off:r.querySelector('[data-k="off"]').checked})); try{$("#matrixState").textContent="Збереження…"; await req("/api/v1/access/matrix",{method:"POST",body:JSON.stringify({...a,userId:user,zones,outputs})}); $("#matrixState").textContent="Збережено";}catch(e){$("#matrixState").textContent=`Помилка: ${e.message}`;}}
  document.addEventListener("click",e=>{if(e.target?.id==="accessRefresh") setTimeout(users,150);});
  const timer=setInterval(()=>{if(ensurePanel()){clearInterval(timer);}},250);
})();
