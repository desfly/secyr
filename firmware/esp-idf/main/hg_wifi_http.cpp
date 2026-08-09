#include "hg_wifi_http.hpp"

#include "esp_log.h"
#include "esp_wifi.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace homeguard::idf {
namespace {
constexpr const char* kTag = "hg_wifi_http";

std::string json_escape(const char* text)
{
    std::string out;
    if (text == nullptr) return out;
    for (const char* p = text; *p != '\0'; ++p) {
        if (*p == '"' || *p == '\\') out.push_back('\\');
        out.push_back(*p);
    }
    return out;
}

constexpr const char kWebUi[] = R"HTML(<!doctype html>
<html lang="uk"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>HomeGuard-S3</title>
<style>
:root{font-family:system-ui,-apple-system,Segoe UI,sans-serif;color:#172033;background:#eef2f7}*{box-sizing:border-box}body{margin:0}header{background:#111827;color:#fff;padding:18px 22px;display:flex;justify-content:space-between;align-items:center}header h1{font-size:21px;margin:0}header small{opacity:.75}.wrap{max-width:1080px;margin:0 auto;padding:20px}.status{display:flex;gap:10px;align-items:center;margin-bottom:18px}.dot{width:11px;height:11px;border-radius:50%;background:#94a3b8}.dot.ok{background:#16a34a}.dot.warn{background:#f59e0b}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:14px}.card{background:#fff;border:1px solid #dbe3ee;border-radius:14px;padding:17px;box-shadow:0 2px 8px #0f172a0a}.card h2{font-size:15px;margin:0 0 14px;color:#475569}.kv{display:grid;grid-template-columns:120px 1fr;gap:8px;font-size:14px}.v{font-weight:650;word-break:break-word}.good{color:#15803d}.bad{color:#b91c1c}.muted{color:#64748b}.wide{grid-column:1/-1}input{width:100%;padding:11px;border:1px solid #cbd5e1;border-radius:9px;margin:5px 0 10px;font-size:14px}button{border:0;border-radius:9px;background:#2563eb;color:white;padding:11px 15px;font-weight:650;cursor:pointer}button.secondary{background:#475569}.row{display:flex;gap:10px;flex-wrap:wrap}.msg{margin-top:10px;font-size:13px}.nets{margin-top:12px;display:grid;gap:7px}.net{display:flex;justify-content:space-between;gap:12px;align-items:center;background:#f8fafc;border:1px solid #dbe3ee;border-radius:9px;padding:9px 11px;cursor:pointer}.net:hover{background:#eff6ff}.net strong{word-break:break-all}.signal{font-size:12px;color:#64748b;white-space:nowrap}footer{text-align:center;color:#64748b;font-size:12px;padding:18px}.pill{display:inline-block;padding:3px 8px;border-radius:99px;background:#e2e8f0;font-size:12px}
</style></head><body>
<header><div><h1>HomeGuard-S3</h1><small>Локальна панель контролера</small></div><span id="buildPill" class="pill">...</span></header>
<main class="wrap"><div class="status"><span id="healthDot" class="dot"></span><strong id="healthText">Завантаження стану...</strong></div><div class="grid">
<section class="card"><h2>Система</h2><div class="kv"><span>Проєкт</span><span class="v" id="project">—</span><span>Build</span><span class="v" id="build">—</span><span>Версія</span><span class="v" id="version">—</span><span>Модуль</span><span class="v" id="module">—</span></div></section>
<section class="card"><h2>Wi‑Fi</h2><div class="kv"><span>STA</span><span class="v" id="sta">—</span><span>Мережа</span><span class="v" id="staSsid">—</span><span>STA IP</span><span class="v" id="staIp">—</span><span>Recovery AP</span><span class="v" id="apSsid">—</span><span>AP IP</span><span class="v" id="apIp">—</span></div></section>
<section class="card"><h2>Фізичні виходи</h2><div class="kv"><span>Runtime</span><span class="v" id="outRuntime">—</span><span>Дозволені</span><span class="v" id="outAllowed">—</span><span>Увімкнені</span><span class="v" id="outEnabled">—</span><span>Помилки</span><span class="v" id="outFailures">—</span></div></section>
<section class="card wide"><h2>Домашня Wi‑Fi мережа</h2><div class="grid"><div><label>SSID</label><input id="ssid" autocomplete="off" placeholder="Назва Wi‑Fi"></div><div><label>Пароль</label><input id="password" type="password" autocomplete="new-password" placeholder="Пароль Wi‑Fi"></div></div><div class="row"><button type="button" onclick="scanWifi()">Пошук мереж Wi‑Fi</button><button type="button" onclick="saveWifi()">Зберегти та підключити</button><button type="button" class="secondary" onclick="refreshAll()">Оновити стан</button></div><div id="wifiMsg" class="msg muted"></div><div id="networks" class="nets"></div></section>
</div></main><footer>HomeGuard-S3 • локальний Web UI • дані оновлюються автоматично</footer>
<script>
const $=id=>document.getElementById(id);async function json(url,opt){const r=await fetch(url,opt);const t=await r.text();if(!r.ok)throw new Error('HTTP '+r.status+' '+t);return t?JSON.parse(t):{};}function yes(v){return v?'ТАК':'НІ'}
async function refreshAll(){let ok=true;try{const b=await json('/api/v1/build');$('project').textContent=b.project||'—';$('build').textContent=b.build||'—';$('version').textContent=b.version||'—';$('module').textContent=b.module||'—';$('buildPill').textContent='Build '+(b.build||'?')}catch(e){ok=false}try{const w=await json('/api/v1/wifi/status');$('sta').textContent=w.station||'—';$('sta').className='v '+(w.station==='connected'?'good':w.station==='connecting'?'':'bad');$('staSsid').textContent=w.station_ssid||'—';$('staIp').textContent=w.station_ip||'—';$('apSsid').textContent=w.ssid||'—';$('apIp').textContent=w.ip||'—'}catch(e){ok=false}try{const o=await json('/api/v1/system/output-runtime');$('outRuntime').textContent=o.runtimeStatus||'—';$('outAllowed').textContent=yes(!!o.outputsAllowed);$('outEnabled').textContent=yes(!!o.outputsEnabled);$('outFailures').textContent=o.failures??'—';$('outAllowed').className='v '+(o.outputsAllowed?'good':'bad')}catch(e){ok=false}$('healthDot').className='dot '+(ok?'ok':'warn');$('healthText').textContent=ok?'Контролер доступний':'Частина сервісів недоступна'}
async function scanWifi(){$('wifiMsg').textContent='Пошук мереж Wi‑Fi...';$('networks').innerHTML='';try{const r=await json('/api/v1/wifi/scan');const a=r.networks||[];$('wifiMsg').textContent=a.length?'Знайдено мереж: '+a.length:'Мереж Wi‑Fi не знайдено';for(const n of a){const d=document.createElement('div');d.className='net';const s=document.createElement('strong');s.textContent=n.ssid||'(прихована мережа)';const q=document.createElement('span');q.className='signal';q.textContent=(n.rssi??'?')+' dBm • ch '+(n.channel??'?');d.append(s,q);if(n.ssid){d.onclick=()=>{$('ssid').value=n.ssid;$('password').focus()}}$('networks').appendChild(d)}}catch(e){$('wifiMsg').textContent='Помилка пошуку: '+e.message}}
async function saveWifi(){const ssid=$('ssid').value.trim(),password=$('password').value;if(!ssid){$('wifiMsg').textContent='Вкажіть SSID';return}$('wifiMsg').textContent='Збереження налаштувань...';try{const r=await json('/api/v1/provisioning/wifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid,password})});$('wifiMsg').textContent='Налаштування прийнято. Підключення до '+ssid+'...';setTimeout(refreshAll,2500)}catch(e){$('wifiMsg').textContent='Зв’язок з Recovery AP змінюється. Перевіряю підключення...';setTimeout(refreshAll,3000)}}
refreshAll();setInterval(refreshAll,3000);
</script></body></html>)HTML";

bool extract_json_string(const std::string& body, const char* key, std::string& value)
{
    const std::string token = std::string{"\""} + key + "\"";
    const auto key_pos = body.find(token); if (key_pos == std::string::npos) return false;
    const auto colon = body.find(':', key_pos + token.size()); if (colon == std::string::npos) return false;
    const auto first_quote = body.find('"', colon + 1); if (first_quote == std::string::npos) return false;
    const auto second_quote = body.find('"', first_quote + 1); if (second_quote == std::string::npos) return false;
    value = body.substr(first_quote + 1, second_quote - first_quote - 1); return true;
}
}

esp_err_t WifiProvisioningHttp::register_handlers(httpd_handle_t server, WifiCredentialStore* store, WifiProvisioningRuntime* runtime)
{
    if (server == nullptr || store == nullptr || runtime == nullptr) return ESP_ERR_INVALID_ARG;
    store_ = store; runtime_ = runtime;
    const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = &WifiProvisioningHttp::root_get, .user_ctx = this},
        {.uri = "/api/v1/provisioning/wifi", .method = HTTP_POST, .handler = &WifiProvisioningHttp::provision_post, .user_ctx = this},
        {.uri = "/api/v1/wifi/status", .method = HTTP_GET, .handler = &WifiProvisioningHttp::status_get, .user_ctx = this},
        {.uri = "/api/v1/wifi/scan", .method = HTTP_GET, .handler = &WifiProvisioningHttp::scan_get, .user_ctx = this},
    };
    for (const auto& route : routes) { const auto error = httpd_register_uri_handler(server, &route); if (error != ESP_OK) return error; }
    return ESP_OK;
}

WifiProvisioningHttp* WifiProvisioningHttp::self_from(httpd_req_t* request){return request == nullptr ? nullptr : static_cast<WifiProvisioningHttp*>(request->user_ctx);}
esp_err_t WifiProvisioningHttp::root_get(httpd_req_t* request){httpd_resp_set_type(request,"text/html; charset=utf-8");return httpd_resp_send(request,kWebUi,static_cast<ssize_t>(sizeof(kWebUi)-1));}

esp_err_t WifiProvisioningHttp::provision_post(httpd_req_t* request)
{
    auto* self=self_from(request); if(self==nullptr||request->content_len==0||request->content_len>512)return httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"invalid provisioning request");
    std::array<char,513> buffer{}; const auto received=httpd_req_recv(request,buffer.data(),request->content_len); if(received<=0)return httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"request body read failed");
    std::string body(buffer.data(),static_cast<std::size_t>(received)),ssid,password;
    if(!extract_json_string(body,"ssid",ssid)||ssid.empty()||ssid.size()>32||!extract_json_string(body,"password",password)||password.size()>64)return httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"ssid/password invalid");
    WifiCredentials credentials{}; std::memcpy(credentials.ssid.data(),ssid.data(),ssid.size()); std::memcpy(credentials.password.data(),password.data(),password.size());
    auto error=self->store_->save(credentials); if(error!=ESP_OK){ESP_LOGE(kTag,"WiFi credentials NVS save failed: %s",esp_err_to_name(error));return httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"NVS save failed");}

    // Acknowledge the browser before the STA reconnect can move the shared AP/STA radio
    // to another channel. Otherwise the Recovery-AP client can lose the POST response
    // and the Web UI reports a misleading "Failed to fetch" even though NVS was saved.
    httpd_resp_set_type(request,"application/json");
    const auto response_error=httpd_resp_send(request,"{\"accepted\":true,\"state\":\"connecting\"}",-1);
    if(response_error!=ESP_OK)return response_error;

    error=self->runtime_->connect_station(credentials.ssid.data(),credentials.password.data());
    if(error!=ESP_OK){
        ESP_LOGE(kTag,"STA connect start failed after provisioning response: %s",esp_err_to_name(error));
    }
    return ESP_OK;
}

esp_err_t WifiProvisioningHttp::status_get(httpd_req_t* request)
{
    auto* self=self_from(request); if(self==nullptr)return ESP_ERR_INVALID_ARG;
    const char* station_state="idle";
    if(self->runtime_->station_connected())station_state="connected";
    else if(self->runtime_->station_connecting())station_state="connecting";

    std::string station_ssid;
    wifi_ap_record_t ap_info{};
    if(self->runtime_->station_connected()&&esp_wifi_sta_get_ap_info(&ap_info)==ESP_OK){
        station_ssid=json_escape(reinterpret_cast<const char*>(ap_info.ssid));
    }

    std::string body="{\"softap\":";
    body+=self->runtime_->started()?"true":"false";
    body+=",\"ssid\":\""; body+=json_escape(self->runtime_->ssid());
    body+="\",\"ip\":\""; body+=json_escape(self->runtime_->ip_address());
    body+="\",\"station\":\""; body+=station_state;
    body+="\",\"station_ssid\":\""; body+=station_ssid;
    body+="\",\"station_ip\":\""; body+=json_escape(self->runtime_->station_ip_address());
    body+="\"}";
    httpd_resp_set_type(request,"application/json"); return httpd_resp_send(request,body.c_str(),static_cast<ssize_t>(body.size()));
}

esp_err_t WifiProvisioningHttp::scan_get(httpd_req_t* request)
{
    wifi_scan_config_t config{};
    auto error=esp_wifi_scan_start(&config,true);
    if(error!=ESP_OK){ESP_LOGE(kTag,"WiFi scan failed: %s",esp_err_to_name(error));return httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"wifi scan failed");}
    std::uint16_t count=0; error=esp_wifi_scan_get_ap_num(&count); if(error!=ESP_OK)return httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"wifi scan count failed");
    count=std::min<std::uint16_t>(count,32); std::vector<wifi_ap_record_t> records(count); std::uint16_t fetched=count;
    if(fetched>0){error=esp_wifi_scan_get_ap_records(&fetched,records.data());if(error!=ESP_OK)return httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"wifi scan records failed");}
    std::string body="{\"networks\":[";
    for(std::uint16_t i=0;i<fetched;++i){if(i)body+=',';body+="{\"ssid\":\"";body+=json_escape(reinterpret_cast<const char*>(records[i].ssid));body+="\",\"rssi\":"+std::to_string(records[i].rssi)+",\"channel\":"+std::to_string(records[i].primary)+"}";}
    body+="]}"; httpd_resp_set_type(request,"application/json"); return httpd_resp_send(request,body.c_str(),static_cast<ssize_t>(body.size()));
}

}  // namespace homeguard::idf
