#pragma once
#include <pgmspace.h>

// Mobile-first web app served by the ESP32. Kept in flash (PROGMEM) so there
// is no LittleFS/SPIFFS upload step.
const char WEB_APP[] PROGMEM = R"GHHTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0a1712">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<meta name="apple-mobile-web-app-title" content="Greenhouse">
<meta name="mobile-web-app-capable" content="yes">
<link rel="manifest" href="/manifest.webmanifest">
<title>Greenhouse</title>
<style>
:root{
  --bg:#0a1712; --bg2:#0e201a; --card:#132a22; --card2:#17342a;
  --line:#245042; --text:#e8f5ee; --muted:#8fb4a4;
  --green:#5ce09a; --blue:#5cc8ff; --amber:#ffbe5c; --red:#ff7b7b;
  --r:16px; --shadow:0 8px 24px rgba(0,0,0,.28);
}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0;padding:0;background:var(--bg);color:var(--text)}
body{
  font:15px/1.45 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;
  padding:calc(env(safe-area-inset-top) + 14px) 14px calc(env(safe-area-inset-bottom) + 26px);
  background-image:radial-gradient(900px 500px at 80% -10%,#174433 0%,transparent 60%);
  min-height:100vh;
}
.app{max-width:860px;margin:0 auto}
header{display:flex;align-items:center;gap:12px;margin-bottom:16px}
.logo{width:42px;height:42px;border-radius:12px;display:grid;place-items:center;
  background:linear-gradient(145deg,#1e6b4b,#0e3a29);border:1px solid #2c7d5b;flex:none}
.logo svg{width:24px;height:24px}
h1{font-size:20px;margin:0;letter-spacing:.2px}
.sub{color:var(--muted);font-size:12px;margin-top:2px}
.dot{width:9px;height:9px;border-radius:50%;background:var(--red);flex:none;
  box-shadow:0 0 0 4px rgba(255,123,123,.12)}
.dot.ok{background:var(--green);box-shadow:0 0 0 4px rgba(92,224,154,.14)}
.status{margin-left:auto;text-align:right}
.status b{display:block;font-size:12px;font-weight:600}
.status small{color:var(--muted);font-size:11px}

.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(148px,1fr));gap:10px;margin-bottom:14px}
.card{background:linear-gradient(160deg,var(--card2),var(--card));border:1px solid var(--line);
  border-radius:var(--r);padding:12px 13px;box-shadow:var(--shadow);position:relative;overflow:hidden}
.card::after{content:"";position:absolute;inset:0 0 auto 0;height:2px;background:var(--accent,var(--green));opacity:.75}
.card .k{font-size:10px;letter-spacing:1.1px;color:var(--muted);font-weight:700}
.card .v{font-size:30px;font-weight:700;line-height:1.15;margin:4px 0 2px;font-variant-numeric:tabular-nums}
.card .v small{font-size:14px;color:var(--muted);font-weight:600;margin-left:2px}
.card .note{font-size:10.5px;color:var(--muted);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.bar{height:5px;background:#0b1e17;border-radius:4px;margin-top:9px;overflow:hidden}
.bar i{display:block;height:100%;width:0;background:var(--accent,var(--green));border-radius:4px;transition:width .5s ease}
.card.temp{--accent:var(--amber)} .card.hum{--accent:var(--blue)}
.card.soil{--accent:var(--green)} .card.light{--accent:#ffe08a}

.panel{background:var(--card);border:1px solid var(--line);border-radius:var(--r);
  padding:14px;margin-bottom:12px;box-shadow:var(--shadow)}
.panel h2{font-size:12px;letter-spacing:1.1px;color:var(--muted);margin:0 0 12px;text-transform:uppercase}
.seg{display:flex;background:#0b1e17;border-radius:12px;padding:4px;gap:4px;margin-bottom:12px}
.seg button{flex:1;min-height:42px;border:0;border-radius:9px;background:transparent;color:var(--muted);
  font-weight:700;font-size:13px;letter-spacing:.6px;cursor:pointer}
.seg button.on{background:var(--green);color:#062015;box-shadow:0 2px 10px rgba(92,224,154,.25)}
.row{display:flex;gap:10px}
.row>*{flex:1;min-width:0}
button.act,button.wide,button.small{
  border:1px solid var(--line);background:var(--card2);color:var(--text);border-radius:12px;
  min-height:52px;font-weight:700;font-size:13px;cursor:pointer;transition:.15s;
  display:flex;align-items:center;justify-content:center;gap:8px}
button.act:active,button.wide:active,button.small:active{transform:scale(.98)}
button.act.on{background:rgba(92,224,154,.14);border-color:var(--green);color:var(--green)}
button.wide{width:100%;margin-top:10px}
button.small{min-height:38px;font-size:12px}
.btnrow+.btnrow{margin-top:10px}
.hint{color:var(--muted);font-size:11px;margin-top:10px}

details.panel{padding:0}
details.panel>summary{cursor:pointer;list-style:none;padding:14px;font-size:12px;letter-spacing:1.1px;
  color:var(--muted);text-transform:uppercase;font-weight:700;display:flex;justify-content:space-between}
details.panel>summary::-webkit-details-marker{display:none}
details.panel>summary::after{content:"+"}
details.panel[open]>summary::after{content:"\2212"}
details.panel .inner{padding:0 14px 14px}
fieldset{border:0;padding:0;margin:0}
label{display:block;font-size:11px;color:var(--muted);margin:10px 0 4px;font-weight:600}
input[type=number],input[type=text],input[type=password],select{
  width:100%;background:#0b1e17;border:1px solid var(--line);color:var(--text);border-radius:10px;
  padding:11px 12px;font-size:16px;outline:none}
input:focus,select:focus{border-color:var(--green)}
.check{display:flex;align-items:center;gap:10px;margin-top:12px;font-size:13px;color:var(--text)}
.check input{width:20px;height:20px;accent-color:var(--green)}
.cols{display:grid;grid-template-columns:1fr 1fr;gap:0 12px}
@media(min-width:560px){.cols{grid-template-columns:repeat(3,1fr)}}
.actions{display:flex;gap:10px;margin-top:16px}
.actions>*{flex:1}
#toast{position:fixed;left:50%;bottom:calc(env(safe-area-inset-bottom) + 22px);transform:translate(-50%,20px);
  background:#e8f5ee;color:#082018;padding:11px 18px;border-radius:30px;font-weight:700;font-size:13px;
  opacity:0;pointer-events:none;transition:.25s;box-shadow:var(--shadow);z-index:9;max-width:90vw;text-align:center}
#toast.show{opacity:1;transform:translate(-50%,0)}
.foot{color:#5f8474;font-size:11px;text-align:center;margin:18px 0 0}
.pill{display:inline-block;padding:3px 9px;border-radius:20px;font-size:10px;font-weight:800;letter-spacing:.6px;
  background:rgba(255,123,123,.14);color:var(--red)}
.pill.on{background:rgba(92,224,154,.16);color:var(--green)}
.pill.warn{background:rgba(255,190,92,.16);color:var(--amber)}
body.no-fan #btnFan{display:none}
</style>
</head>
<body>
<div class="app">
  <header>
    <div class="logo">
      <svg viewBox="0 0 24 24" fill="none" stroke="#8ff0bd" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">
        <path d="M11 20A7 7 0 0 1 9.8 6.1C15.5 5 17 4.48 19 2c1 2 2 4.18 2 8 0 5.5-4.78 10-10 10Z"/>
        <path d="M2 21c0-3 1.85-5.36 5.08-6C9.5 14.52 12 13 13 12"/>
      </svg>
    </div>
    <div><h1>Greenhouse</h1><div class="sub" id="modeText">--</div></div>
    <div class="status"><span class="dot" id="dot"></span><b id="netText">connecting</b><small id="ip">--</small></div>
  </header>

  <section class="grid">
    <div class="card temp"><div class="k">TEMPERATURE</div><div class="v"><span id="vT">--</span><small>°C</small></div>
      <div class="bar"><i id="bT"></i></div><div class="note" id="nT">fan 27–30 °C</div></div>
    <div class="card hum"><div class="k">HUMIDITY</div><div class="v"><span id="vH">--</span><small>%</small></div>
      <div class="bar"><i id="bH"></i></div><div class="note" id="nH">fan 72–80 %</div></div>
    <div class="card soil"><div class="k">SOIL MOISTURE</div><div class="v"><span id="vS">--</span><small>%</small></div>
      <div class="bar"><i id="bS"></i></div><div class="note" id="nS">water below 35 %</div></div>
    <div class="card light"><div class="k">LIGHT</div><div class="v"><span id="vL">--</span><small>%</small></div>
      <div class="bar"><i id="bL"></i></div><div class="note" id="nL">raw <span id="rawL">--</span></div></div>
  </section>

  <section class="panel">
    <h2>Controls</h2>
    <div class="seg" id="segMode">
      <button data-mode="auto">AUTO</button>
      <button data-mode="manual">MANUAL</button>
    </div>
    <div class="row btnrow">
      <button class="act" id="btnPump"><span>💧</span><span>PUMP</span><span class="pill" id="pPump">IDLE</span></button>
      <button class="act" id="btnFan"><span>🌀</span><span>FAN</span><span class="pill" id="pFan">OFF</span></button>
    </div>
    <button class="wide" id="btnWater">Water now</button>
    <div class="row btnrow"><button class="small" id="btnScreen">Cycle OLED screen</button><button class="small" id="btnRefresh">Refresh</button></div>
    <div class="hint" id="waterHint">Pump is capped at 8 s and then rests while the soil soaks.</div>
  </section>

  <details class="panel" id="setBox"><summary>Settings &amp; calibration</summary><div class="inner">
    <fieldset class="cols">
      <div><label>Water below (%)</label><input id="moist" type="number" min="0" max="100" step="1"></div>
      <div><label>Pump run (seconds)</label><input id="pumpSec" type="number" min="1" max="120" step="1"></div>
      <div><label>Soak time (minutes)</label><input id="soakMin" type="number" min="0" max="120" step="1"></div>
      <div><label>Temp fan ON (°C)</label><input id="tOn" type="number" min="0" max="60" step="0.5"></div>
      <div><label>Temp fan OFF (°C)</label><input id="tOff" type="number" min="0" max="60" step="0.5"></div>
      <div><label>Humidity fan ON (%)</label><input id="hOn" type="number" min="0" max="100" step="1"></div>
      <div><label>Humidity fan OFF (%)</label><input id="hOff" type="number" min="0" max="100" step="1"></div>
      <div><label>Soil RAW in dry air</label><input id="dry" type="number" min="0" max="4095" step="1"></div>
      <div><label>Soil RAW in water</label><input id="wet" type="number" min="0" max="4095" step="1"></div>
      <div><label>Bright light limit (%)</label><input id="lightPct" type="number" min="0" max="100" step="1"></div>
    </fieldset>
    <label class="check"><input id="block" type="checkbox"> Block watering in bright midday light</label>
    <label class="check"><input id="invert" type="checkbox"> Invert light sensor (if bright reads low)</label>
    <label class="check"><input id="relayInv" type="checkbox"> Invert relay logic (tick if pump is ON when it should be OFF)</label>
    <div class="actions">
      <button class="small" id="calDry">Use current RAW as DRY</button>
      <button class="small" id="calWet">Use current RAW as WET</button>
    </div>
    <div class="actions"><button class="small" id="saveSet">Save settings</button></div>
    <div class="hint">Tip: put the probe in dry air, tap "Use current RAW as DRY", then in water and tap WET, then Save.</div>
  </div></details>

  <details class="panel" id="wifiBox"><summary>WiFi setup</summary><div class="inner">
    <div class="hint" id="wifiInfo" style="margin:0 0 8px"></div>
    <label>Network name (SSID)</label><input id="ssid" type="text" autocomplete="off">
    <label>Password</label><input id="pass" type="password" autocomplete="off">
    <div class="actions"><button class="small" id="saveWifi">Save &amp; restart</button></div>
    <div class="hint">If the ESP32 cannot reach your network it opens the <b>GREENHOUSE-SETUP</b> access point. Connect your phone to it and this page opens at 192.168.4.1.</div>
  </div></details>

  <p class="foot">ESP32 greenhouse controller · updates every 2 s</p>
</div>
<div id="toast"></div>
<script>
const $=id=>document.getElementById(id);
let last=null,cfgLoaded=false,toastTimer=null;

function toast(m){const e=$('toast');e.textContent=m;e.className='show';clearTimeout(toastTimer);toastTimer=setTimeout(()=>e.className='',2300);}
async function api(u){try{const r=await fetch(u,{cache:'no-store'});if(!r.ok)throw 0;return await r.json();}catch(e){$('dot').className='dot';toast('Connection lost');return null;}}
function num(v,d){return typeof v==='number'&&isFinite(v)?v.toFixed(d):'--';}
function clamp(v){return Math.max(0,Math.min(100,Number(v)||0));}
function bar(id,v){$(id).style.width=clamp(v)+'%';}
function pill(id,on,label,onLabel){const e=$(id);e.textContent=on?(onLabel||'ON'):label;e.className='pill'+(on?' on':'');}

function fillSettings(c){
  if(!c)return;
  $('moist').value=c.moist; $('pumpSec').value=(c.pumpMs/1000).toFixed(0);
  $('soakMin').value=(c.soakMs/60000).toFixed(0);
  $('tOn').value=c.tempOn; $('tOff').value=c.tempOff;
  $('hOn').value=c.humOn; $('hOff').value=c.humOff;
  $('dry').value=c.soilDry; $('wet').value=c.soilWet;
  $('lightPct').value=c.lightBlockPct;
  $('block').checked=!!c.lightBlock; $('invert').checked=!!c.ldrInvert;
  $('relayInv').checked=(c.relayActiveLow===false);
}
function render(d){
  last=d;
  $('vT').textContent=num(d.t,1); $('vH').textContent=num(d.h,0);
  $('vS').textContent=num(d.soil,0); $('vL').textContent=num(d.light,0);
  $('rawL').textContent=d.lightRaw;
  bar('bT',(Number(d.t)||0)/50*100); bar('bH',d.h); bar('bS',d.soil); bar('bL',d.light);
  const c=d.cfg||{};
  const hasFan = d.fanEnabled!==false;
  document.body.classList.toggle('no-fan',!hasFan);
  $('nT').textContent=(hasFan?'fan ':'target ')+c.tempOff+'\u2013'+c.tempOn+' C';
  $('nH').textContent=(hasFan?'fan ':'target ')+c.humOff+'\u2013'+c.humOn+' %';
  $('nS').textContent='water below '+c.moist+' %';
  pill('pPump',d.pump,'IDLE','RUNNING');
  pill('pFan',d.fan,'OFF','ON');
  $('modeText').textContent=d.mode+(d.ap?'  \u00b7  setup mode':'');
  $('netText').textContent=d.ap?'SETUP ACCESS POINT':'WIFI CONNECTED';
  $('ip').textContent=d.ip+(d.rssi!==null?'  \u00b7  '+d.rssi+' dBm':'');
  $('dot').className='dot ok';
  document.querySelectorAll('#segMode button').forEach(b=>b.classList.toggle('on',b.dataset.mode===d.mode.toLowerCase()));
  $('btnPump').classList.toggle('on',!!d.pump);
  $('btnFan').classList.toggle('on',!!d.fan);
  $('waterHint').textContent = d.pump ? 'Watering... '+d.remain+' s left' :
    'Pump capped at '+(c.pumpMs/1000)+' s \u00b7 next check in '+d.remain+' s';
  $('wifiInfo').textContent = d.ap ? 'Phone is connected to the setup AP. Enter your home WiFi below.' :
    'Connected to WiFi at '+d.ip+'. Enter new credentials to switch networks.';
  if(!cfgLoaded){fillSettings(c);cfgLoaded=true;}
}
async function refresh(){const d=await api('/api/state');if(d)render(d);}
async function cmd(c){const d=await api('/api/command?cmd='+encodeURIComponent(c));if(d&&d.ok){toast('Done');refresh();}}
function val(id){return $(id).value;}
$('segMode').onclick=e=>{const b=e.target.closest('button');if(b)cmd('mode_'+b.dataset.mode);};
$('btnPump').onclick=()=>cmd('pump_toggle');
$('btnFan').onclick=()=>cmd('fan_toggle');
$('btnWater').onclick=()=>cmd('water_now');
$('btnScreen').onclick=()=>cmd('screen');
$('btnRefresh').onclick=()=>refresh();
$('calDry').onclick=()=>{if(last){$('dry').value=last.soilRaw;toast('DRY raw captured \u2013 tap Save');}};
$('calWet').onclick=()=>{if(last){$('wet').value=last.soilRaw;toast('WET raw captured \u2013 tap Save');}};
$('saveSet').onclick=async()=>{
  const q=new URLSearchParams({moist:val('moist'),pumpSec:val('pumpSec'),soakMin:val('soakMin'),
    tOn:val('tOn'),tOff:val('tOff'),hOn:val('hOn'),hOff:val('hOff'),dry:val('dry'),wet:val('wet'),
    lightPct:val('lightPct'),block:$('block').checked?1:0,invert:$('invert').checked?1:0,
    relayInv:$('relayInv').checked?1:0});
  const d=await api('/api/settings?'+q.toString());
  if(d&&d.ok){toast('Settings saved');cfgLoaded=false;refresh();}
};
$('saveWifi').onclick=async()=>{
  if(!val('ssid')){toast('Enter a network name');return;}
  const q=new URLSearchParams({ssid:val('ssid'),pass:val('pass')});
  const d=await api('/api/wifi?'+q.toString());
  if(d&&d.ok){toast('Saved \u2013 restarting');setTimeout(()=>location.reload(),6000);}
};
$('setBox').addEventListener('toggle',()=>{cfgLoaded=false;refresh();});
$('wifiBox').addEventListener('toggle',()=>{if(last)$('ssid').value=last.ssid||'';});
document.addEventListener('visibilitychange',()=>{if(!document.hidden)refresh();});
refresh();setInterval(refresh,2000);
</script>
</body>
</html>
)GHHTML";

const char MANIFEST[] PROGMEM = R"GHJSON({
  "name":"Greenhouse Controller",
  "short_name":"Greenhouse",
  "start_url":"/",
  "display":"standalone",
  "background_color":"#0a1712",
  "theme_color":"#0a1712",
  "icons":[]
}
)GHJSON";
