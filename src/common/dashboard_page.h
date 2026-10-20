#pragma once
#include <Arduino.h>

// Single-page dashboard served at "/". Polls /api a few times per second.
// Add to the iPhone home screen (Share -> Add to Home Screen) to open it
// full-screen like an app.
static const char kDashboardPage[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<meta name="apple-mobile-web-app-title" content="E92">
<meta name="theme-color" content="#000000">
<title>E92 Dashboard</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
html,body{height:100%;background:#000;color:#eee;font-family:-apple-system,system-ui,sans-serif;-webkit-user-select:none;user-select:none}
body{display:flex;flex-direction:column;padding:max(8px,env(safe-area-inset-top)) max(8px,env(safe-area-inset-right)) max(6px,env(safe-area-inset-bottom)) max(8px,env(safe-area-inset-left))}
#g{flex:1;display:grid;grid-template-columns:repeat(4,minmax(0,1fr));grid-auto-rows:1fr;gap:8px}
@media (orientation:portrait){#g{grid-template-columns:repeat(2,minmax(0,1fr))}}
.c{background:#111;border-radius:14px;padding:8px 12px;display:flex;flex-direction:column;justify-content:space-between;min-height:0;min-width:0;overflow:hidden;transition:opacity .3s}
.c.st{opacity:.35}
.l{font-size:13px;color:#888;text-transform:uppercase;letter-spacing:.06em}
.v{font-size:min(7vw,12vh);font-weight:600;font-variant-numeric:tabular-nums;line-height:1.05;white-space:nowrap}
@media (orientation:portrait){.v{font-size:min(9vw,6vh)}}
.v small{font-size:.35em;color:#888;font-weight:400;margin-left:4px}
.r{font:11px ui-monospace,Menlo,monospace;color:#6a6;display:none;white-space:nowrap;overflow:hidden}
body.raw .r{display:block}
#s{display:flex;flex-wrap:wrap;gap:4px 14px;font-size:12px;color:#777;padding-top:6px;align-items:center}
#s b{font-weight:500;color:#aaa;font-variant-numeric:tabular-nums}
#can{display:inline-block;min-width:6ch}
#s .sp{flex:1}
#s button,#s a{text-decoration:none;background:#222;color:#aaa;border:0;border-radius:8px;padding:4px 10px;font-size:12px}
.bad{color:#e55!important}
</style></head><body>
<div id="g"></div>
<div id="s"><span>Wi-Fi <b id="net">–</b></span><span>Phone <b id="ph">–</b></span><span>CAN <b id="can">–</b></span><span>Reset <b id="rst">–</b></span><span class="sp"></span><span id="ver"></span><a href="/can">sniffer</a><a href="/update">update</a><button id="rb">raw</button></div>
<script>
const NAMES={speed:"Speed",rpm:"RPM",coolant:"Coolant",outside:"Outside",battery:"Battery",fuel:"Fuel",range:"Range"};
const STALE=3000,g=document.getElementById("g"),cards={};
let btnCard=null;
function card(key,label){const c=document.createElement("div");c.className="c st";
c.innerHTML='<div class="l">'+label+'</div><div class="v">–</div><div class="r"></div>';g.appendChild(c);
return cards[key]={c:c,v:c.querySelector(".v"),r:c.querySelector(".r")};}
function show(k,txt,unit,stale,raw){const o=cards[k];o.v.innerHTML=txt+(unit?"<small>"+unit+"</small>":"");
o.c.classList.toggle("st",stale);if(raw!==undefined)o.r.textContent=raw;}
function render(d){
for(const f of d.f){if(!cards[f.k])card(f.k,NAMES[f.k]||f.k);
const stale=f.age<0||f.age>STALE;const txt=f.v===null?"–":f.v.toFixed(f.d);
show(f.k,txt,f.u,stale,"0x"+f.id+": "+(f.raw||"no frame"));}
if(!btnCard)btnCard=card("btn","Last button");
show("btn",d.btn||"–","",d.btnAge<0||d.btnAge>10000,d.btnAge<0?"":(d.btnAge/1000).toFixed(1)+" s ago");
document.getElementById("ver").textContent="v"+d.ver;const rs=document.getElementById("rst");rs.textContent=d.reset;rs.classList.toggle("bad",d.resetFault);const net=document.getElementById("net");net.textContent="connected";net.classList.remove("bad");
const ph=document.getElementById("ph");ph.textContent=d.phone?"connected":"waiting";ph.classList.toggle("bad",!d.phone);
const can=document.getElementById("can");const ca=d.canAge<0||d.canAge>2000;
can.textContent=ca?"none":d.fps+"/s";can.classList.toggle("bad",ca);}
let busy=false;
async function poll(){if(busy)return;busy=true;const ac=new AbortController();const t=setTimeout(()=>ac.abort(),1500);
try{const r=await fetch("/api",{cache:"no-store",signal:ac.signal});render(await r.json());}
catch(e){const n=document.getElementById("net");n.textContent="offline";n.classList.add("bad");
for(const k in cards)cards[k].c.classList.add("st");}
finally{clearTimeout(t);busy=false;}}
setInterval(poll,250);poll();
document.getElementById("rb").onclick=()=>document.body.classList.toggle("raw");
let wl=null;async function wake(){try{if(navigator.wakeLock&&!wl){wl=await navigator.wakeLock.request("screen");wl.onrelease=()=>wl=null;}}catch(e){}}
document.addEventListener("visibilitychange",()=>{if(document.visibilityState==="visible")wake();});
document.addEventListener("click",wake);wake();
</script></body></html>)HTML";

static const char kUpdatePage[] PROGMEM = R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="apple-mobile-web-app-capable" content="yes">
<title>E92 firmware update</title>
<style>body{background:#000;color:#eee;font-family:-apple-system,system-ui,sans-serif;padding:24px}
a{color:#8ab4ff}button,input{font-size:16px;margin-top:12px}
progress{width:100%;height:14px;margin-top:16px}#m{margin-top:12px;color:#aaa}</style></head><body>
<a href="/">&larr; Dashboard</a>
<h2>Firmware update</h2>
<p>Running: <b id="cv">…</b></p>
<p>Select <code>firmware.bin</code> (from <code>.pio/build/app_a2dp_esp32/</code>, or
<code>.pio/build/app_ble_esp32c3/</code> for the C3 build,
e.g. via AirDrop or iCloud Drive). The board restarts when done.</p>
<form id="f"><input type="file" id="fw" accept=".bin" required><br>
<button type="submit" id="go">Upload</button></form>
<progress id="p" max="100" value="0" hidden></progress><div id="m"></div>
<script>
fetch("/api").then(r=>r.json()).then(d=>document.getElementById("cv").textContent="v"+d.ver).catch(()=>{});
const m=document.getElementById("m"),p=document.getElementById("p"),go=document.getElementById("go");
document.getElementById("f").onsubmit=e=>{
 e.preventDefault();const file=document.getElementById("fw").files[0];if(!file)return;
 const fd=new FormData();fd.append("firmware",file,file.name);
 const x=new XMLHttpRequest();x.open("POST","/update");
 x.upload.onprogress=ev=>{if(ev.lengthComputable){p.value=ev.loaded*100/ev.total;m.textContent="Uploading "+Math.round(p.value)+"%";}};
 x.onload=()=>{if(x.status===200){p.value=100;m.textContent="Done, restarting… back to the dashboard in 10 s";setTimeout(()=>location.href="/",10000);}
  else{m.textContent="Failed ("+x.status+"): "+x.responseText;go.disabled=false;}};
 x.onerror=()=>{m.textContent="Connection lost during upload";go.disabled=false;};
 go.disabled=true;p.hidden=false;m.textContent="Uploading…";x.send(fd);
};
</script>
</body></html>)HTML";

// Live CAN sniffer at "/can": every seen ID with its latest bytes; bytes that
// changed in the last second are highlighted. Polls /canapi.
static const char kSnifferPage[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<title>E92 CAN sniffer</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#000;color:#ddd;font-family:-apple-system,system-ui,sans-serif;padding:max(8px,env(safe-area-inset-top)) max(8px,env(safe-area-inset-right)) 8px max(8px,env(safe-area-inset-left))}
#t{display:flex;flex-wrap:wrap;gap:8px;align-items:center;font-size:13px;margin-bottom:8px}
#t a,#t button,#t input{background:#222;color:#ccc;border:0;border-radius:8px;padding:6px 10px;font-size:13px;text-decoration:none}
#t input{width:90px;font-family:ui-monospace,Menlo,monospace}
#t button.on{background:#2a4a7a;color:#fff}
#info{color:#777}
#bus{font:12px ui-monospace,Menlo,monospace;color:#777;margin-bottom:8px}
#bus .bad{color:#e55}
table{width:100%;border-collapse:collapse;font:13px ui-monospace,Menlo,monospace}
th{position:sticky;top:0;background:#000;color:#777;font-weight:400;text-align:left;padding:4px 6px}
td{padding:3px 6px;border-top:1px solid #151515;white-space:nowrap}
td.n{text-align:right;color:#888}
.d span{display:inline-block;width:2.3ch;margin-right:.4ch}
.d span.c{background:#7a5a10;color:#fff;border-radius:3px}
tr.old{opacity:.35}
</style></head><body>
<div id="t"><a href="/">&larr; Dash</a><input id="f" placeholder="ID filter" autocapitalize="characters" autocomplete="off">
<button id="ch">changed only</button><button id="pa">pause</button><button id="rs">reset</button><span id="info"></span></div>
<div id="bus"></div>
<table><thead><tr><th>ID</th><th>Data</th><th class="n">Hz</th><th class="n">Count</th></tr></thead><tbody id="b"></tbody></table>
<script>
const b=document.getElementById("b"),info=document.getElementById("info"),f=document.getElementById("f");
let onlyCh=false,paused=false,reset=false,prev={},prevT=0;
const tog=(id,fn)=>{const el=document.getElementById(id);el.onclick=()=>{el.classList.toggle("on",fn());};};
tog("ch",()=>onlyCh=!onlyCh);
tog("pa",()=>paused=!paused);
document.getElementById("rs").onclick=()=>{reset=true;prev={};};
async function poll(){
 try{
  if(!paused||reset){
   const r=await fetch("/canapi"+(reset?"?reset=1":""),{cache:"no-store"});reset=false;
   const d=await r.json(),now=performance.now(),dt=(now-prevT)/1000,flt=f.value.trim().toUpperCase();
   let rows=[],shown=0;
   d.ids.sort((x,y)=>x[0]-y[0]);
   for(const [id,ext,hex,chg,cnt,age] of d.ids){
    const ids=id.toString(16).toUpperCase().padStart(ext?8:3,"0");
    const key=ids+(ext?"x":"");
    const hz=prev[key]!==undefined&&dt>0?Math.max(0,(cnt-prev[key])/dt):0;
    prev[key]=cnt;
    if(flt&&!ids.includes(flt))continue;
    if(onlyCh&&!chg)continue;
    let bytes="";
    for(let i=0;i<hex.length/2;i++)bytes+='<span'+(chg>>i&1?' class="c"':'')+'>'+hex.substr(i*2,2)+'</span>';
    rows.push('<tr'+(age>3000?' class="old"':'')+'><td>'+ids+'</td><td class="d">'+bytes+'</td><td class="n">'+(hz<10?hz.toFixed(1):Math.round(hz))+'</td><td class="n">'+cnt+'</td></tr>');
    shown++;
   }
   prevT=now;
   b.innerHTML=rows.join("");
   info.textContent=shown+" / "+d.ids.length+" IDs"+(d.full?" (table full)":"");
   if(d.bus){const[r,e,m]=d.bus,c=v=>v?' class="bad"':'';
    document.getElementById("bus").innerHTML='CAN <span'+c(!r)+'>'+(r?'running':'stopped')+'</span> · bus errors <span'+c(e)+'>'+e+'</span> · missed <span'+c(m)+'>'+m+'</span>';}
  }
 }catch(e){info.textContent="offline";}
 setTimeout(poll,500);
}
poll();
</script></body></html>)HTML";
