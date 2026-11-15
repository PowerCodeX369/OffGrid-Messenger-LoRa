'use strict';
/* OffGrid Messenger LoRa v1.6.0: encrypted text over SX1278 / USB Serial.
   Adapted from the original OffGrid web client, preserving its OGM4 AES-GCM codec. */
const P=globalThis.OGP,$=id=>document.getElementById(id);
const MAX_CHARS=500,MAX_PHOTO=48*1024,MAX_VOICE_MS=120000,STORE='OffGridMessengerV14';
const APP_VERSION='1.10.0-rc4-ultra-sync-diagnostics';
const Media=globalThis.OffGridLoRaMedia;
/* Played is a separate state for voice notes: reading the chat must never
 * imply that audio was listened to. Message tick status remains unchanged. */
const stateRank={sending:0,queued:0,waiting:0,sent:1,failed:1,delivered:2,read:3};
const encoder=new TextEncoder();
let port=null,reader=null,writer=null,readPromise=null,closing=false,verified=false,serialBuffer='',writeChain=Promise.resolve(),handshakeTimer=null;
let handshakeAttempts=0;
let localMac=null,secretKey=null,peerMac=null,activeContact='Nearby node',firmwareMismatchNotified=false;
/* Profile changes require a user click. Firmware negotiates over authenticated
 * existing-profile radio frames, and both peers revert if not confirmed. */
let currentProfile='BALANCED', profileTransition=false, autoMode=false, autoBusy=false;
let profileRadio={sf:7,bw:250000,power:10};
globalThis.OffGridUltraActive=()=>currentProfile==='ULTRA';
const profileLabels={LOW:'Fast / Nominal 18 dBm',BALANCED:'Balanced',MAX:'Max Power / Range',ULTRA:'Ultra Long Range · text only'};
let highPaVerified=false;
let mediaResponseWaiter=null;

let pendingNodeGet=null,pendingNodeSet=null;
/* USB diagnostics: distinguish missing console output from incompatible or old firmware. */
const usbLog=[];
let keyFingerprint=null; // 48-bit SHA-256 display only; NEVER store or log the key
let usbWaitTimer=null,usbHandshakeStarted=0,usbLastRawAt=0,usbHandshakeIssue='',usbSawBeacon=false;
function logUsb(line){usbLog.push(`[${new Date().toLocaleTimeString()}] ${String(line).slice(0,450)}`);if(usbLog.length>240)usbLog.shift();}
/* Preserve RF counters at the END of the long LINK record. Previously the
 * first 240 characters hid PEER_ARQ, CRC, ACK, and USB errors, precisely the
 * fields needed to investigate one-way reception. Never log KEYDATA. */
function compactLinkDiagnostic(line){
 const fields={};const bits=line.split('|');
 for(const token of bits.slice(2)){const i=token.indexOf('=');if(i>0)fields[token.slice(0,i)]=token.slice(i+1);}
 const names=['RSSI','PEER_AGE_MS','RFHEARD','PROFILE','AUTO','PEER_ARQ',
  'TXOK','TXFAIL','RX','ARQ_TX','ARQ_ACKTX','ARQ_ACKRX',
  'ARQ_RETX','ARQ_TIMEOUT','ARQ_BADMAC','ARQ_USBERR','CRC','BADTAG'];
 return 'LINK|'+(bits[1]||'NONE')+'|'+names.map(k=>k+'='+(fields[k]??'?')).join('|');
}
async function radioKeyFingerprint(raw){
 if(!(raw instanceof Uint8Array)||raw.length!==32)return null;
 const digest=new Uint8Array(await crypto.subtle.digest('SHA-256',raw));
 return Array.from(digest.slice(0,6),b=>b.toString(16).padStart(2,'0')).join('').toUpperCase();
}
function showUsbHint(message){usbHandshakeIssue=message;const area=$('usbDiagnostics');if(!area)return;area.classList.remove('hidden');$('usbDiagnosticsText').textContent=message;}
function clearUsbHint(){usbHandshakeIssue='';$('usbDiagnostics')?.classList.add('hidden');}
function updateUsbWait(){
 if(!port||verified)return;
 const elapsed=performance.now()-usbHandshakeStarted;
 if(elapsed<5000||usbHandshakeIssue)return;
 if(!usbLastRawAt)showUsbHint('USB port opened, but no serial data. Check ESP-IDF USB Serial/JTAG console and reboot the node.');
 else if(usbSawBeacon)showUsbHint('ESP32 sends READY, but has not confirmed the STATUS command. Check the USB input path.');
 else if(!usbHandshakeIssue)showUsbHint('ESP32 serial output detected, but no compatible READY response. Open Diagnostics.');
}
function diagnoseUsb(){
 showDialog('USB diagnostics',body=>{
  body.append(el('p','',`State: ${verified?'connected':port?'waiting for READY':'disconnected'}. ${usbHandshakeIssue||'No active USB error.'}`));
  body.append(el('p','',`Pairing key fingerprint (SHA-256 prefix): ${keyFingerprint||'not loaded'} — compare with the OTHER ESP32. Different fingerprints mean encryption and radio HMAC cannot work, even if both browsers show Connected.`));
  const log=el('div','usb-diagnostics-log',usbLog.join('\n')||'No serial output captured.');body.append(log);
  const controls=el('div','modal-actions');
  const copy=el('button','quiet-button','Copy diagnostics');copy.onclick=()=>navigator.clipboard?.writeText(usbLog.join('\n')+'\n'+usbHandshakeIssue).then(()=>toast('Copied USB diagnostic log')).catch(()=>toast('Clipboard unavailable'));
  const retry=el('button','primary-button','Send STATUS again');retry.onclick=()=>{if(!writer){toast('Connect your ESP32 first');return;}void writeLine('STATUS').then(()=>{logUsb('HOST> STATUS (manual)');toast('STATUS sent');}).catch(e=>toast(e.message));};
  controls.append(copy,retry);body.append(controls);
  body.append(el('p','',`Expected: READY|MAC|…|PROTO=2|CAP=180|STABLE=1|NVSKEY=1. Radio counters: last received peer ${radioTelemetry.peer}, TX queued ${radioTelemetry.txok}, TX errors ${radioTelemetry.txfail}, RX ${radioTelemetry.rx}. At stable broadcast rate, peer RSSI appears after a valid incoming radio packet.`));
 });
}

let db=null,records=[],recordMap=new Map(),photoUrls=new Map(),voiceUrls=new Map(),incoming=new Map();
const rxGuard=new globalThis.OffGridLoRaRxGuard.AuthenticatedRxGuard();
let currentVoiceRecording=null,voiceTimer=null;
const mediaReassembler=new Media.Reassembler(4);
let transferQueue=[],activeTransfer=null,runningTransfers=false;
/* Photo-only selective ACK. Groups are per sender MAC and ESP32 boot-session;
 * IDs remain arbitrary (do not assume sequential or shared with other traffic). */
const sackGroups=new Map();
const SACK_GROUP_MAX=6, SACK_FLUSH_MS=75;
let selectivePhotoAcks=true;

const radioByHex=new Map(),radioByWire=new Map();
let rfScheduler=new globalThis.OffGridLoRaScheduler.FragmentScheduler(64);
let bulkLink=null;

const MAX_TEXT_QUEUE=24;
const linkStats={samples:0,avgMs:0,cleanStreak:0,retries:0,mode:'starting',window:1,lastKBs:0};
let linkPreference='auto';
const photoFlow=new globalThis.OffGridAdaptive.PhotoFlowController();
let adaptivePhotos=true;
let queuedReceipts=new Set(),pendingReads=new Set(),theme='dark';
const receiptWatchdogs=new Map();
const weakTextRetry=globalThis.OffGridWeakTextQueue.make();
let weakTextTimer=null;
let freshLoRaAt=0;
let peerIsFresh=false;

let imageMode='hq';
/* Browser-only desktop notifications. Consent requires an explicit user click.
   No keys or private message content are ever stored in notification settings. */
let desktopNotificationsEnabled=false, desktopNotificationPreview=false;
const recentlyNotified=new Set();
const APP_TITLE='OffGrid Messenger LoRa';
let radioTelemetry={peer:'NONE',rssi:null,phy:1,mode:'AUTO',delivered:0,retries:0,failures:0,txok:0,txfail:0,rx:0,hello:0,healthGood:0,pingOk:0,pingLost:0,available:false};
let selectedPhotoId=null,viewerZoomed=false,viewerTouchX=null,viewerPreviousFocus=null;
/* Isolated benchmark state; measurements never enter message history. */
const speedDiagnostics={busy:false,cancelRequested:false,serial:null,full:null,results:{},output:null,runButton:null,capable:false,sackCapable:false};
const benchToken=()=>crypto.getRandomValues(new Uint32Array(1))[0]||1;
const kbps=(bytes,ms)=>bytes/1024/Math.max(.001,ms/1000);
function diagnosticLine(line){
 const parts=line.split('|'),type=parts[0],test=speedDiagnostics.serial;
 if(type==='USBOUT'&&test?.kind==='usbOut'&&Number(parts[1])===test.token){
  const seq=Number(parts[2]),hex=parts[3]||'';
  if(seq===test.count&&/^[a-fA-F0-9]{320}$/.test(hex)){
   test.count++;test.bytes+=hex.length/2;
  }
  return true;
 }
 if(type==='USBOUT_DONE'){
  if(test?.kind==='usbOut'&&Number(parts[1])===test.token){
   const bytes=Number(parts[3]),elapsed=performance.now()-test.started;
   if(test.count===Number(parts[2])&&test.bytes===bytes)test.resolve({bytes,ms:elapsed,nodeMs:Number(parts[4]),packets:test.count});
   else test.reject(Error(`Received ${test.count} of ${parts[2]} USB packets`));
  }
  return true;
 }
 if(type==='USBIN_DONE'){
  if(test?.kind==='usbIn'&&Number(parts[1])===test.token){
   const bytes=Number(parts[3]),packets=Number(parts[2]);
   if(bytes===test.expectedBytes&&packets===test.expectedPackets)test.resolve({bytes,ms:performance.now()-test.started,nodeMs:Number(parts[4]),packets});
   else test.reject(Error(`USB input short: ${bytes}/${test.expectedBytes} bytes, ${packets}/${test.expectedPackets} packets`));
  }
  return true;
 }
 if(type==='RBENCH_START')return true;
 if(type==='RBENCH_ERROR'){
  if(test?.kind==='radio'&&Number(parts[1])===test.token)test.reject(Error(parts[2]||'Radio benchmark failed'));
  return true;
 }
 if(type==='RBENCH_DONE'){
  if(test?.kind==='radio'&&Number(parts[1])===test.token){
   test.resolve({requested:Number(parts[2]),ok:Number(parts[3]),lost:Number(parts[4]),retries:Number(parts[5]),
     bytes:Number(parts[6]),ms:Number(parts[7]),rttMs:Number(parts[8]),phy:Number(parts[9]),rssi:Number(parts[10])});
  }
  return true;
 }
 return false;
}
function diagPrint(message){
 if(speedDiagnostics.output){speedDiagnostics.output.textContent+=message+'\n';speedDiagnostics.output.scrollTop=speedDiagnostics.output.scrollHeight;}
}
function showSpeedDiagnostics(){
 showDialog('LoRa speed & reliability test',body=>{
  body.append(el('p','','Encrypted end-to-end LoRa test, not a theoretical radio bitrate. Both ESP32s must run v1.6 and both browser clients must be connected. Test data is random, and nothing is added to chat history. Normal messages and receipts always take priority.'));
  const controls=el('div','settings-actions');
  const sizeLabel=el('label','','Payload per test (bytes)');
  const size=document.createElement('select');
  for(const n of [64,256,512,1024]){const opt=document.createElement('option');opt.value=String(n);opt.textContent=n+' bytes';size.append(opt);}
  size.value='256';sizeLabel.append(size);
  const countLabel=el('label','','Number of test messages');
  const count=document.createElement('select');
  for(const n of [4,8,12]){const opt=document.createElement('option');opt.value=String(n);opt.textContent=String(n);count.append(opt);}
  count.value='8';countLabel.append(count);
  const start=el('button','primary-button','Start LoRa speed test');
  const cancel=el('button','quiet-button','Cancel test');cancel.disabled=true;
  const log=el('pre','usb-diagnostics-log','Ready. Make sure the second browser is connected.');
  start.onclick=()=>void runLoraSpeedTest(Number(size.value),Number(count.value),start,log,cancel);
  cancel.onclick=()=>{speedDiagnostics.cancelRequested=true;speedDiagnostics.full?.reject(Error('Benchmark canceled'));cancel.disabled=true;};
  controls.append(sizeLabel,countLabel,start,cancel);
  body.append(controls,log);
  const d=el('button','quiet-button','USB diagnostics');d.onclick=diagnoseUsb;body.append(d);
 });
}
async function runLoraSpeedTest(size,count,button,outputNode,cancelButton){
 if(speedDiagnostics.busy)return;
 if(!verified||!secretKey){toast('Connect and pair the LoRa node first');return;}
 if(transferQueue.length||activeTransfer||rfScheduler.depth||urgentReceipts.length){toast('Finish existing transfers first');return;}
 speedDiagnostics.busy=true;speedDiagnostics.cancelRequested=false;button.disabled=true;cancelButton.disabled=false;
 const run={results:[],started:performance.now(),size,count};
 outputNode.textContent=`Starting encrypted LoRa benchmark · ${profileLabels[currentProfile]} · SF${profileRadio.sf}/BW${profileRadio.bw/1000}/TX${profileRadio.power}dBm…\n`;
 try{
  for(let i=0;i<count;i++){
   if(speedDiagnostics.cancelRequested)throw Error('Benchmark canceled');
   if(!verified)throw Error('USB disconnected');
   const job={kind:'probe',control:false,probeId:P.newId(),size,fragmentRTTs:[],started:performance.now()};
   let timer=null;
   const promise=new Promise((resolve,reject)=>{
    speedDiagnostics.full={job,resolve,reject};
    timer=setTimeout(()=>reject(Error('Missing encrypted probe reply')),24000+Math.ceil(size/160)*2500);
   });
   transferQueue.push(job);void runTransfers();
   try{
    const result=await promise;
    run.results.push({ok:true,ms:result.ms,bytes:size,frames:result.frames});
    const m=job.bulkMetrics;
    outputNode.textContent+=`Test ${i+1}/${count}: delivered + confirmed in ${(result.ms/1000).toFixed(2)} s`+
      (m?` | local USB ${(m.usbStageMs/1000).toFixed(2)}s; RF confirm ${(m.radioWaitMs/1000).toFixed(2)}s; ARQ retransmissions ${m.RETX??'n/a'}`:'')+'\n';
   }catch(e){
    if(speedDiagnostics.cancelRequested)throw Error('Benchmark canceled');
    run.results.push({ok:false,ms:performance.now()-job.started,bytes:0});
    outputNode.textContent+=`Test ${i+1}/${count}: unconfirmed (${e.message})\n`;
   }finally{
    if(timer!==null)clearTimeout(timer);
    if(speedDiagnostics.full?.job===job)speedDiagnostics.full=null;
   }
  }
  const stats=globalThis.OffGridLoRaBenchmark.summarize(run.results,performance.now()-run.started);
  outputNode.textContent+='\n--- Results ---\n'+
   `Confirmed: ${stats.ok}/${stats.total}; unconfirmed: ${stats.lost} (${stats.lossPct.toFixed(1)}%)\n`+
   `Application goodput (including failed trials): ${stats.goodputBps.toFixed(1)} bytes/s (${(stats.goodputBps/1024).toFixed(3)} KiB/s)\n`+
   `Confirmed message RTT median: ${stats.medianMs===null?'N/A':(stats.medianMs/1000).toFixed(2)+' s'}\n`+
   `Confirmed message RTT p95: ${stats.p95Ms===null?'N/A':(stats.p95Ms/1000).toFixed(2)+' s'}\n`+
   `Wall time: ${(stats.elapsedMs/1000).toFixed(1)} s\n`+
   'Local USB and RF-confirmation timings are measured separately; the full round trip also includes the remote browser, encrypted reply, and other overhead. These are not RF PHY bitrates.\n';
 }catch(e){outputNode.textContent+='Test stopped: '+e.message+'\n';}
 finally{speedDiagnostics.busy=false;speedDiagnostics.cancelRequested=false;button.disabled=false;cancelButton.disabled=true;speedDiagnostics.full=null;}
}


try{theme=localStorage.getItem('offgrid_theme')==='light'?'light':'dark';desktopNotificationsEnabled=localStorage.getItem('offgrid_desktop_notifications')==='on';desktopNotificationPreview=localStorage.getItem('offgrid_notification_previews')==='on';imageMode=localStorage.getItem('offgrid_image_mode')==='lossless'?'lossless':'hq';linkPreference=['auto','range','speed','lr500','lr250'].includes(localStorage.getItem('offgrid_link_preference'))?localStorage.getItem('offgrid_link_preference'):'auto';adaptivePhotos=localStorage.getItem('offgrid_adaptive_photos')!=='off';selectivePhotoAcks=localStorage.getItem('offgrid_sack_photos')!=='off';}catch{}
document.body.classList.toggle('light',theme==='light');
const macKey=()=>localMac||'unknown';
const nameKey=()=>`offgrid_phase14_name_${macKey()}`;
const fmtTime=t=>new Date(t||Date.now()).toLocaleTimeString([],{hour:'2-digit',minute:'2-digit'});
const countChars=s=>Array.from(s).length;
const wait=ms=>new Promise(r=>setTimeout(r,ms));
const formatBytes=n=>n<1024?`${n} B`:`${(n/1024).toFixed(1)} KB`;
function toast(text){const d=document.createElement('div');d.className='toast';d.textContent=text;$('toastHost').append(d);setTimeout(()=>d.remove(),4200);}
function isVisible(){return !document.hidden&&document.hasFocus()&&(innerWidth>590||document.body.classList.contains('mobile-chat'));}

/* Unread is a local viewing state; readSent means a radio receipt reached the
   sender, which can legitimately be delayed by a weak LR link. Do not mix them. */
function unreadTotal(){return records.reduce((count,r)=>count+(r.dir==='in'&&!r.seenLocally?1:0),0);}
function updateUnreadBadge(){
 const count=unreadTotal(),label=count>99?'99+':String(count);
 for(const id of ['unreadBadge','contactUnreadBadge']){
  const element=$(id);if(!element)continue;
  element.hidden=count===0;element.textContent=label;
  element.setAttribute('aria-label',count===1?'1 unread message':`${count} unread messages`);
 }
 document.title=count?`(${count}) ${APP_TITLE}`:APP_TITLE;
 const contact=$('contactButton');if(contact)contact.classList.toggle('has-unread',count>0);
}
function notificationAvailable(){return typeof Notification!=='undefined'&&window.isSecureContext;}
function notificationActive(){return notificationAvailable()&&Notification.permission==='granted'&&desktopNotificationsEnabled;}
function updateNotificationButton(){
 const btn=$('notificationButton');if(!btn)return;
 const active=notificationActive();btn.classList.toggle('notifications-active',active);
 btn.setAttribute('aria-pressed',String(active));
 btn.title=active?'Disable desktop alerts':'Enable desktop alerts';
 btn.setAttribute('aria-label',active?'Disable desktop notifications':'Enable desktop notifications');
}
async function toggleDesktopNotifications(){
 if(notificationActive()){
  desktopNotificationsEnabled=false;
  try{localStorage.setItem('offgrid_desktop_notifications','off');}catch{}
  updateNotificationButton();toast('Desktop notifications turned off.');return;
 }
 if(!notificationAvailable()){
  toast('Desktop notifications require Chromium on localhost or HTTPS.');return;
 }
 // This function is called only from an actual click. Do not request permission
 // during page load or when receiving an unsolicited radio packet.
 let permission=Notification.permission;
 if(permission==='default'){
  try{permission=await Notification.requestPermission();}
  catch(e){toast('Could not request notification permission.');return;}
 }
 if(permission!=='granted'){
  desktopNotificationsEnabled=false;updateNotificationButton();
  toast('Notification permission is blocked; check Chromium site settings.');return;
 }
 desktopNotificationsEnabled=true;
 try{localStorage.setItem('offgrid_desktop_notifications','on');}catch{}
 updateNotificationButton();toast('Desktop notifications enabled.');
}
function notificationTextFor(r){
 if(r.kind==='image')return 'An encrypted photo arrived';
 if(r.kind==='voice')return 'An encrypted voice note arrived';
 return desktopNotificationPreview?(r.text||'New message').slice(0,110):'New encrypted message';
}
function notifyIncoming(r){
 if(r.dir!=='in'||!notificationActive()||(!document.hidden&&document.hasFocus()))return;
 if(recentlyNotified.has(r.id))return;
 recentlyNotified.add(r.id);
 if(recentlyNotified.size>120)recentlyNotified.delete(recentlyNotified.values().next().value);
 try{
  const notice=new Notification(APP_TITLE,{
   body:notificationTextFor(r),icon:new URL('logo.svg',document.baseURI).href,
   tag:'offgrid-'+r.id
  });
  notice.onclick=()=>{
   try{window.focus();document.body.classList.add('mobile-chat');scrollToNewest(true);sendReadReceipts();notice.close();}catch(e){console.warn('Notification activation',e);}
  };
 }catch(e){console.warn('Native desktop notification unavailable:',e);}
}

function safeText(s){return typeof s==='string'?s:'';}
function openDb(){return new Promise((resolve,reject)=>{
 const req=indexedDB.open(STORE,1);
 req.onupgradeneeded=()=>{const x=req.result;if(!x.objectStoreNames.contains('keys'))x.createObjectStore('keys',{keyPath:'mac'});if(!x.objectStoreNames.contains('messages')){const st=x.createObjectStore('messages',{keyPath:'pk'});st.createIndex('owner','owner',{unique:false});}};
 req.onsuccess=()=>resolve(req.result);req.onerror=()=>reject(req.error);
});}
function txRequest(store,mode,make){return new Promise((resolve,reject)=>{
 const tx=db.transaction(store,mode),st=tx.objectStore(store);let req;
 try{req=make(st);}catch(e){reject(e);return;}
 tx.oncomplete=()=>resolve(req?.result);tx.onerror=()=>reject(tx.error);tx.onabort=()=>reject(tx.error||Error('Database transaction aborted'));
});}
const dbGet=(store,key)=>txRequest(store,'readonly',st=>st.get(key));
const dbPut=(store,value)=>txRequest(store,'readwrite',st=>st.put(value));
const dbDelete=(store,key)=>txRequest(store,'readwrite',st=>st.delete(key));
const listStored=()=>txRequest('messages','readonly',st=>st.index('owner').getAll(IDBKeyRange.only(macKey())));
async function wipeStored(){const rows=await listStored();await new Promise((resolve,reject)=>{const tx=db.transaction('messages','readwrite'),st=tx.objectStore('messages');for(const row of rows)st.delete(row.pk);tx.oncomplete=resolve;tx.onerror=()=>reject(tx.error);});}
/* One browser RF transaction at a time. The firmware also enforces one
 * unacknowledged LoRa packet at a time. Receipt frames get first priority. */
function chosenWindow(job){return 1;} /* Exactly one unacknowledged radio fragment, INCLUDING receipts. */

function photoBudget(){return MAX_PHOTO;}

function updateLinkBadge(){
 const node=$('linkModeLabel');if(!node)return;
 const signal=radioTelemetry.rssi===null?'RSSI unknown':`${radioTelemetry.rssi} dBm`;
 node.textContent=`${autoMode?'AUTO · ':''}${profileLabels[currentProfile]||currentProfile} · SF${profileRadio.sf} · BW ${profileRadio.bw/1000} kHz · ${profileRadio.power} dBm · ${signal}`;
 node.title=`Manual LoRa profile, not distance detection. Authenticated frames: ${radioTelemetry.rx}. Peer: ${radioTelemetry.peer}. Retries: ${radioTelemetry.retries}.`;
}

function recordLinkResult(rtt,attempts,isPhoto=false){
 if(isPhoto)photoFlow.success(rtt,attempts);
 linkStats.samples++;
 linkStats.avgMs=linkStats.samples===1?rtt:linkStats.avgMs*0.78+rtt*0.22;
 if(attempts>1||rtt>1800){linkStats.cleanStreak=0;linkStats.retries++;linkStats.mode='weak';}
 else{
   linkStats.cleanStreak++;
   if(linkStats.cleanStreak>=12&&linkStats.avgMs<420)linkStats.mode='strong';
   else if(linkStats.cleanStreak>=5)linkStats.mode='steady';
 }
 linkStats.window=chosenWindow();updateLinkBadge();
}
function recordLinkFailure(){photoFlow.penalty('radio or browser ACK failure');linkStats.cleanStreak=0;linkStats.mode='weak';linkStats.window=1;linkStats.retries++;updateLinkBadge();}
function pendingRadio(){return radioByHex.size>0 || Boolean(bulkLink?.active);}
function clearSlot(slot){
 if(!slot)return;clearTimeout(slot.timer);
 if(radioByHex.get(slot.hex)===slot)radioByHex.delete(slot.hex);
 if(slot.wire&&radioByWire.get(slot.wire)===slot)radioByWire.delete(slot.wire);
}
function failPendingRadio(error){for(const slot of [...radioByHex.values()]){clearSlot(slot);slot.reject(error);}}
function refreshStatus(){
 const ready=verified&&!!secretKey;
 $('statusDot').className='status-dot'+(verified?' on':port?' wait':'');
 $('connectionState').textContent=verified?'Connected':port?'Checking firmware':'Disconnected';
 $('connectionDescription').textContent=verified?(localMac+' · SX1278 LoRa · protocol 2'):port?(usbHandshakeIssue||'Waiting for ESP32 READY'):'Connect your ESP32-C3 over USB';
 $('nodeAddress').textContent='Node: '+(localMac||'—');
 $('connectButton').disabled=!!port;$('disconnectButton').disabled=!port;
 $('messageInput').disabled=!ready;const textOnly=currentProfile==='ULTRA';
 $('attachments').disabled=!ready||textOnly;$('photoPicker').disabled=!ready||textOnly;$('voiceButton').disabled=!ready||textOnly;
 $('messageInput').placeholder=ready?'Message (up to 500 characters)':verified?'Pair a 256-bit key to begin':'Connect ESP32 to send messages';
 $('chatSubhead').textContent=ready?'End-to-end encrypted · SX1278 LoRa':verified?'Pair a key to start encrypted chat':'Offline · connect ESP32';
 $('securityIcon').textContent=ready?'🔒':'♢';
 $('securityLabel').textContent=ready?'AES-256-GCM enabled · manage pairing':verified?'Encryption key required · set up pairing':'Connect to configure encryption';
 $('composerStatus').textContent=ready?(currentProfile==='ULTRA'?'Ultra SF12 · encrypted TEXT ONLY · media disabled':'Encrypted text / photos / voice · LoRa v1.8'):verified?'Pair the same key on both browsers to begin':'Connect an ESP32 using the sidebar';
 updateByteCount();updateLinkBadge();
}
function setName(name){activeContact=(name||'Nearby node').slice(0,36);$('contactSidebarName').textContent=activeContact;$('chatName').textContent=activeContact;const initial=Array.from(activeContact)[0]?.toUpperCase()||'N';$('contactAvatar').textContent=initial;$('headerAvatar').textContent=initial;updateContactAvatar();}
const avatarDbKey=()=>`avatar/${macKey()}`;
async function updateContactAvatar(){
 let data='';
 if(secretKey&&db&&localMac){
  try{
   const row=await dbGet('keys',avatarDbKey());
   if(row?.seal)data=P.textFrom(await P.openAtRest(secretKey,row.seal,'contact-avatar',macKey()));
  }catch(e){console.warn('Contact photo cannot be decrypted:',e.message);}
 }
 for(const id of ['contactAvatar','headerAvatar']){
  const node=$(id);if(!node)continue;
  node.style.backgroundImage=/^data:image\/jpeg;base64,[A-Za-z0-9+/=]+$/.test(data)?`url("${data}")`:'';
  node.classList.toggle('has-contact-avatar',!!node.style.backgroundImage);
 }
}
function chooseContactAvatar(){
 const picker=document.createElement('input');picker.type='file';picker.accept='image/*';
 picker.onchange=async()=>{const file=picker.files?.[0];if(!file)return;
  if(file.size>12*1024*1024){toast('Choose a photo smaller than 12 MB');return;}
  try{
   const bitmap=await createImageBitmap(file);const canvas=document.createElement('canvas');canvas.width=160;canvas.height=160;
   const ctx=canvas.getContext('2d');if(!ctx)throw Error('Canvas unavailable');
   const crop=Math.min(bitmap.width,bitmap.height);
   ctx.drawImage(bitmap,(bitmap.width-crop)/2,(bitmap.height-crop)/2,crop,crop,0,0,160,160);
   bitmap.close?.();const url=canvas.toDataURL('image/jpeg',.74);
   if(url.length>48_000)throw Error('Contact picture is too large');
   if(!secretKey||!verified||!db)throw Error('Connect and pair your node first');
   const sealed=await P.sealAtRest(secretKey,P.textBytes(url),'contact-avatar',macKey());
   await dbPut('keys',{mac:avatarDbKey(),seal:sealed});
   void updateContactAvatar();toast('Encrypted contact photo saved on this browser');
  }catch(e){toast('Could not save avatar: '+e.message);}
 };
 picker.click();
}
function persistName(){try{localStorage.setItem(nameKey(),activeContact);}catch{}}
function updateSidebar(){const r=records[records.length-1];$('lastChatPreview').textContent=r?(r.dir==='out'?'You: ':'')+(r.kind==='image'?'📷 Photo':r.kind==='voice'?'🎤 Voice message':r.text||'Message'):'Your encrypted off-grid conversation';$('lastChatTime').textContent=r?fmtTime(r.ts):'';$('introCard').style.display=records.length?'none':'';}
function createReceipt(status){const wrap=document.createElement('span');wrap.className='receipt-icon '+(status||'sending');wrap.dataset.role='tick';wrap.setAttribute('aria-label',status||'Sending');
 wrap.innerHTML='<svg viewBox="0 0 26 16" width="26" height="16" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path class="tick-one" d="M1.5 8.1 6.1 12.5 16.4 2.1"/><path class="tick-two" d="M8.5 8.1 13.1 12.5 23.4 2.1"/></svg>';return wrap;}
/* Message viewport follow mode.  A normal scrollTop assignment happens before
 * decoded image dimensions become available, so ResizeObserver keeps the newest
 * message visible as the chat grows.  A deliberate upward user scroll pauses
 * resizing-based auto-follow; a genuinely new message resumes it. */
const chatScrollState = {follow:true, frame:0, touchY:null};
function chatNearBottom(){
 const view=$('chatScroll');
 return view.scrollHeight - view.clientHeight - view.scrollTop < 88;
}
function scrollToNewest(force=false){
 if(force)chatScrollState.follow=true;
 if(!chatScrollState.follow || chatScrollState.frame)return;
 chatScrollState.frame=requestAnimationFrame(()=>{
  chatScrollState.frame=0;
  if(!chatScrollState.follow)return;
  const view=$('chatScroll');
  view.scrollTop=view.scrollHeight;
 });
}
function setupChatAutoScroll(){
 const view=$('chatScroll');
 // Wheel / touch / keyboard scrolling is a deliberate choice to read history.
 view.addEventListener('wheel',event=>{
  if(event.deltaY < -2)chatScrollState.follow=false;
  else if(chatNearBottom())chatScrollState.follow=true;
 },{passive:true});
 view.addEventListener('touchstart',event=>{
  chatScrollState.touchY=event.touches[0]?.clientY??null;
 },{passive:true});
 view.addEventListener('touchmove',event=>{
  if(chatScrollState.touchY===null)return;
  const y=event.touches[0]?.clientY??chatScrollState.touchY;
  if(y>chatScrollState.touchY+8)chatScrollState.follow=false;
  else if(chatNearBottom())chatScrollState.follow=true;
  chatScrollState.touchY=y;
 },{passive:true});
 view.addEventListener('scroll',()=>{
  if(chatNearBottom())chatScrollState.follow=true;
 },{passive:true});
 view.addEventListener('keydown',event=>{
  if(['PageUp','Home','ArrowUp'].includes(event.key))chatScrollState.follow=false;
  if(['End'].includes(event.key)){chatScrollState.follow=true;scrollToNewest(true);}
 });
 if('ResizeObserver' in window){
  new ResizeObserver(()=>scrollToNewest()).observe($('messages'));
 }
}
setupChatAutoScroll();
function makeBubble(record,animate=true){
 const row=document.createElement('div');row.className='message-row '+(record.dir==='out'?'out':'in')+(animate?' is-new':'');row.dataset.mid=record.id;row.dataset.status=record.status||'';
 const bubble=document.createElement('div');bubble.className='message-bubble';
 if(record.deleted){
   bubble.append(el('div','message-deleted','⊘  This message was deleted'));
 }else if(record.kind==='image'){
   const img=document.createElement('img');img.className='chat-photo';img.alt='Shared photograph';img.loading=animate?'eager':'lazy';img.addEventListener('load',()=>scrollToNewest());
   const mime=record.mime||'image/webp',url=URL.createObjectURL(new Blob([record.bytes],{type:mime}));photoUrls.set(record.id,url);img.src=url;
   img.tabIndex=0;img.setAttribute('role','button');img.setAttribute('aria-label','Open photograph in gallery');
   img.onclick=()=>openPhotoViewer(record.id);img.onkeydown=e=>{if(e.key==='Enter'||e.key===' '){e.preventDefault();openPhotoViewer(record.id);}};
   bubble.append(img);
   const photoCaption=document.createElement('div');photoCaption.className='photo-caption';photoCaption.textContent=`${record.width||'?'} × ${record.height||'?'} · ${formatBytes(record.bytes?.length||0)}`;bubble.append(photoCaption);
 }else if(record.kind==='voice'){
   const container=el('div','voice-clip'),sound=document.createElement('audio');
   const mic=el('span','voice-played-icon');mic.dataset.role='voice-played';
   mic.innerHTML='<svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="1.9" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="9" y="2" width="6" height="13" rx="3"/><path d="M5.5 11a6.5 6.5 0 0 0 13 0M12 17.5V22m-4 0h8"/></svg>';
   container.append(mic);
   sound.controls=true;sound.preload='none';sound.setAttribute('aria-label','Voice message playback');
   if(record.bytes?.length){
     /* 'play' only signals a requested state change. 'playing' means the
        browser has actually started playback; no autoplay or read shortcut. */
     sound.addEventListener('playing',()=>{void markVoicePlayed(record.id);});
   }
   if(record.bytes?.length){
     const mime=record.mime||'audio/webm';
     const url=URL.createObjectURL(new Blob([record.bytes],{type:mime}));
     voiceUrls.set(record.id,url);sound.src=url;container.append(sound);
   }else container.append(el('span','voice-empty','Voice recording unavailable'));
   const duration=el('span','voice-clip-info',`${Math.round((record.durationMs||0)/1000)}s`);
   container.append(duration);bubble.append(container);
 }else{const content=document.createElement('div');content.className='message-text';content.textContent=record.text;bubble.append(content);}
 if(record.edited&&!record.deleted)bubble.append(el('span','message-edited','edited'));
 if(record.mutationFailed)bubble.append(el('span','message-sync-error','Update not synced'));
 const meta=document.createElement('div');meta.className='message-meta';const time=document.createElement('span');time.textContent=fmtTime(record.ts);meta.append(time);
 if(record.dir==='out'){const progress=document.createElement('span');progress.className='transfer-progress';progress.dataset.role='progress';progress.textContent=record.status==='sending'&&record.progress?record.progress+'%':'';meta.append(progress,createReceipt(record.status||'sending'));}
 bubble.append(meta);row.append(bubble);$('messages').append(row);updateBubble(record);
 // Context menu and long-press support, without hijacking photo taps or audio play.
 row.addEventListener('contextmenu',e=>{e.preventDefault();showMessageActions(record.id);});
 let timer=null,startX=0,startY=0;
 row.addEventListener('touchstart',e=>{
  if(e.touches.length!==1)return;startX=e.touches[0].clientX;startY=e.touches[0].clientY;
  clearTimeout(timer);timer=setTimeout(()=>{timer=null;showMessageActions(record.id);},560);
 },{passive:true});
 row.addEventListener('touchmove',e=>{if(timer&&e.touches[0]&&
  (Math.abs(e.touches[0].clientX-startX)>16||Math.abs(e.touches[0].clientY-startY)>16)){
   clearTimeout(timer);timer=null;
 }},{passive:true});
 for(const event of ['touchend','touchcancel'])row.addEventListener(event,()=>{clearTimeout(timer);timer=null;});
 if(animate)scrollToNewest(true);return row;
}
function updateBubble(r){const row=Array.from($('messages').children).find(el=>el.dataset.mid===r.id);if(!row)return;
 const previousStatus=row.dataset.status;row.dataset.status=r.status||'';
 if(r.dir==='out'&&previousStatus&&previousStatus!==r.status&&['delivered','read'].includes(r.status)){
  row.classList.remove('receipt-pop');void row.offsetWidth;row.classList.add('receipt-pop');
  setTimeout(()=>row.classList.remove('receipt-pop'),530);
 }
 const receipt=row.querySelector('[data-role="tick"]');if(receipt){receipt.className='receipt-icon '+(r.status||'sending');receipt.setAttribute('aria-label',r.status||'Sending');}
 const voiceIcon=row.querySelector('[data-role="voice-played"]');if(voiceIcon){
   voiceIcon.classList.toggle('played',!!r.played);
   voiceIcon.setAttribute('aria-label',r.played?'Voice message played':'Voice message not yet played');
   voiceIcon.title=r.played?'Voice message played':'Voice message not yet played';
 }
 const prog=row.querySelector('[data-role="progress"]');if(prog)prog.textContent=r.status==='sending'&&r.progress?`${r.progress}%${['image','voice'].includes(r.kind)&&r.speedKbps?' · '+r.speedKbps.toFixed(1)+' KB/s':''}`:'';
 if(r.dir==='out'){row.title=(r.status||'Sending')+(r.error?': '+r.error:'');row.classList.toggle('retryable',['failed','waiting'].includes(r.status));if(['failed','waiting'].includes(r.status))row.onclick=()=>retryMessage(r.id);else row.onclick=null;}
}
function revokePhotos(){closePhotoViewer();for(const url of photoUrls.values())URL.revokeObjectURL(url);photoUrls.clear();
 for(const url of voiceUrls.values())URL.revokeObjectURL(url);voiceUrls.clear();}
function renderHistory(){revokePhotos();$('messages').replaceChildren();for(const r of records)makeBubble(r,false);updateSidebar();updateUnreadBadge();scrollToNewest(true);}
/* In-app viewer. Photo blobs are already authenticated, decrypted and locally stored. */
function galleryPhotos(){return records.filter(r=>r.kind==='image'&&r.bytes?.length>0);}
function showSelectedPhoto(animated=true){
 const gallery=galleryPhotos(),index=gallery.findIndex(r=>r.id===selectedPhotoId);
 if(index<0){closePhotoViewer();return;}
 const photo=gallery[index],image=$('viewerImage');
 const url=photoUrls.get(photo.id);
 if(!url){closePhotoViewer();return;}
 viewerZoomed=false;$('viewerCanvas').classList.remove('zoomed');
 image.src=url;
 $('photoViewerCounter').textContent=`${index+1} of ${gallery.length}`;
 $('photoViewerInfo').textContent=`${photo.width||'?'} × ${photo.height||'?'} · ${formatBytes(photo.bytes.length)}`;
 $('photoViewerName').textContent=activeContact;
 $('photoViewerDate').textContent=fmtTime(photo.ts);
 $('viewerPrevious').disabled=index<=0;
 $('viewerNext').disabled=index>=gallery.length-1;
 $('viewerPrevious').classList.toggle('viewer-hidden',index<=0);
 $('viewerNext').classList.toggle('viewer-hidden',index>=gallery.length-1);
 $('viewerDownload').href=url;
 $('viewerDownload').download=`offgrid-${new Date(photo.ts).toISOString().slice(0,10)}.${photo.mime==='image/png'?'png':photo.mime==='image/jpeg'?'jpg':'webp'}`;
 if(animated){image.classList.remove('photo-enter');void image.offsetWidth;image.classList.add('photo-enter');}
}
function openPhotoViewer(id){
 if(!photoUrls.has(id))return;
 if($('photoViewer').classList.contains('hidden'))viewerPreviousFocus=document.activeElement;
 selectedPhotoId=id;$('photoViewer').classList.remove('hidden');
 document.body.classList.add('viewer-open');showSelectedPhoto(true);$('photoViewer').focus();
}
function closePhotoViewer(){
 if($('photoViewer').classList.contains('hidden'))return;
 $('photoViewer').classList.add('hidden');document.body.classList.remove('viewer-open');
 $('viewerImage').removeAttribute('src');selectedPhotoId=null;
 if(viewerPreviousFocus?.isConnected)viewerPreviousFocus.focus({preventScroll:true});
 viewerPreviousFocus=null;
}
function navigatePhoto(delta){
 if($('photoViewer').classList.contains('hidden'))return;
 const gallery=galleryPhotos();const index=gallery.findIndex(r=>r.id===selectedPhotoId);
 const next=index+delta;
 if(next>=0&&next<gallery.length){selectedPhotoId=gallery[next].id;showSelectedPhoto(true);}
}
function togglePhotoZoom(){
 if($('photoViewer').classList.contains('hidden'))return;
 viewerZoomed=!viewerZoomed;$('viewerCanvas').classList.toggle('zoomed',viewerZoomed);
}
function serializeRecord(r){return {edited:!!r.edited,deleted:!!r.deleted,mutationFailed:!!r.mutationFailed,id:r.id,kind:r.kind,dir:r.dir,ts:r.ts,text:r.text||'',mime:r.mime||'',width:r.width||0,height:r.height||0,durationMs:r.durationMs||0,bytes:r.bytes?P.base64url(r.bytes):'',readSent:!!r.readSent,deliveredAckSent:!!r.deliveredAckSent,played:!!r.played,playedSent:!!r.playedSent,seenLocally:!!r.seenLocally,peerMac:r.peerMac||null};}
async function storeRecord(r){if(!secretKey||!localMac)return;const payload=P.textBytes(JSON.stringify(serializeRecord(r)));const sealed=await P.sealAtRest(secretKey,payload,macKey(),r.id);await dbPut('messages',{pk:macKey()+'/'+r.id,owner:macKey(),id:r.id,ts:r.ts,dir:r.dir,status:r.status,seal:sealed});}
async function storeStatus(r){if(!secretKey||!localMac)return;const old=await dbGet('messages',macKey()+'/'+r.id);if(old){old.status=r.status;await dbPut('messages',old);}else await storeRecord(r);}
async function hydrateHistory(){records=[];recordMap.clear();const rows=await listStored();for(const row of rows){try{const bytes=await P.openAtRest(secretKey,row.seal,macKey(),row.id);const d=JSON.parse(P.textFrom(bytes));d.bytes=d.bytes?P.fromBase64urlRaw(d.bytes):null;d.status=row.status||'sent';d.edited=!!d.edited;d.deleted=!!d.deleted;d.mutationFailed=!!d.mutationFailed;d.seenLocally=typeof d.seenLocally==='boolean'?d.seenLocally:!!d.readSent;if(d.status==='sending'||d.status==='queued')d.status=d.kind==='text'?'waiting':'failed';
   if(d.kind==='text'&&d.dir==='out'&&d.status==='failed')d.status='waiting';d.progress=0;records.push(d);recordMap.set(d.id,d);}catch(e){console.warn('Cannot decrypt stored record',row.id,e.message);}}
 records.sort((a,b)=>a.ts-b.ts);renderHistory();}
/* Stored image data is base64url; unlike pairing keys, image byte length is arbitrary. */
P.fromBase64urlRaw=function(s){const fixed=s.replace(/-/g,'+').replace(/_/g,'/');const bin=atob(fixed+'='.repeat((4-fixed.length%4)%4));return Uint8Array.from(bin,c=>c.charCodeAt(0));};
async function appendRecord(r){if(r.dir==='in'&&typeof r.seenLocally!=='boolean')r.seenLocally=false;records.push(r);recordMap.set(r.id,r);makeBubble(r);updateSidebar();updateUnreadBadge();try{await storeRecord(r);}catch(e){toast('Could not store encrypted chat history: '+e.message);} }
/* Played receipts are encrypted application messages, independent of read
 * ticks. A replay or repeated playback cannot enqueue duplicate receipts.
 * If delivery fails, playedSent remains false for retry after reconnect. */
async function markVoicePlayed(id){
 const r=recordMap.get(id);
 /* A sender previewing their own audio must NOT turn the outgoing mic blue;
  * it only becomes blue after the other browser reports actual playback. */
 if(!r||r.dir!=='in'||r.kind!=='voice'||!r.bytes?.length)return;
 if(!r.played){r.played=true;updateBubble(r);try{await storeRecord(r);}catch(e){console.warn('Played state save failed',e);}}
 if(r.dir==='in'&&!r.playedSent&&verified&&secretKey)enqueueReceipt('played',r.id);
}
async function setState(r,status,error=''){
 if(!r)return;
 if(['delivered','read'].includes(status)){
  weakTextRetry.clear(r.id);
  const t=receiptWatchdogs.get(r.id);
  if(t){clearTimeout(t);receiptWatchdogs.delete(r.id);}
 }
 const prior=r.status||'sending';
 // An old failed retry, disconnect callback or watchdog must NEVER undo
 // a genuine authenticated delivered/read receipt.
 if(prior==='read'&&status!=='read')return;
 if(prior==='delivered'&&!['delivered','read'].includes(status))return;
 if(status!=='failed'&&status!=='waiting'&&(stateRank[status]??0)<(stateRank[prior]??0))return;
 r.status=status;r.error=error;r.progress=status==='sending'?r.progress:0;updateBubble(r);
 if(r.dir==='out'&&r.kind==='text'&&r.textClickedAt&&prior!==status&&
    (status==='delivered'||status==='read')){
  logUsb(`TEXTRECEIPT|${status.toUpperCase()}|RTT_MS=${Math.round(performance.now()-r.textClickedAt)}`);
 }
 try{await storeStatus(r);}catch(e){console.warn('Status save failed',e);}
}
async function setProgress(r,pct){if(!r||['sent','delivered','read','failed'].includes(r.status))return;r.status='sending';r.progress=Math.max(0,Math.min(100,pct));updateBubble(r);}
/* Radio TxDone is ONE gray tick; only an authenticated OGM4 delivered
 * receipt creates TWO gray ticks. A lost encrypted receipt must leave an
 * explicit unconfirmed result rather than inventing successful delivery. */
function watchDeliveryReceipt(r,fragments){
 if(!r || !verified || ['delivered','read'].includes(r.status))return;
 const old=receiptWatchdogs.get(r.id);if(old)clearTimeout(old);
 const deadline=globalThis.OffGridLoRaDelivery.receiptTimeoutMs(fragments,currentProfile==='ULTRA');
 receiptWatchdogs.set(r.id,setTimeout(()=>{
  receiptWatchdogs.delete(r.id);
  if(verified && !['delivered','read'].includes(r.status)){
   if(r.kind==='text'){
    weakTextRetry.postpone(r.id,Date.now());
    void setState(r,'waiting','No encrypted delivery receipt yet; retry when LoRa peer is heard');
   }else void setState(r,'failed','Encrypted delivery receipt unconfirmed; recipient may already have it');
  }
 },deadline));
}
/* Durable encrypted text outbox: only one transfer at a time, only when a
 * recently authenticated LoRa peer was heard. Re-encrypts with fresh transfer
 * IDs; duplicate message IDs are suppressed at the receiving application. */
function clearWeakRetry(){
 if(weakTextTimer!==null){clearInterval(weakTextTimer);weakTextTimer=null;}
 peerIsFresh=false;freshLoRaAt=0;
}
async function pumpWeakText(){
 if(!verified||!secretKey||!db||!writer||!peerIsFresh||Date.now()-freshLoRaAt>30000||
    runningTransfers||activeTransfer||transferQueue.length||pendingRadio()||profileTransition)return;
 const now=Date.now();
 const r=records.find(x=>x.dir==='out'&&x.kind==='text'&&
    ['waiting','sent'].includes(x.status)&&weakTextRetry.due(x.id,now,true)&&
    (x.status==='waiting'||now-x.ts>=(currentProfile==='ULTRA'?240000:90000)));
 if(!r)return;
 weakTextRetry.attempt(r.id,now);
 await setState(r,'sending');
 transferQueue.push(makeJob(r));
 void runTransfers();
}
function startWeakRetry(){
 if(weakTextTimer===null)weakTextTimer=setInterval(()=>{
  void pumpWeakText().catch(e=>console.warn('Deferred encrypted text',e));
 },12000);
}
function writeLine(line){const task=writeChain.then(async()=>{if(!writer)throw Error('USB not connected');await writer.write(encoder.encode(line+'\n'));});writeChain=task.catch(()=>{});return task;}
/* KEYGET/KEYSET are local USB commands. Raw key material is never saved in
   IndexedDB/localStorage. The CryptoKey only lives in this tab's RAM. */
function keyRequest(kind,command){
 return new Promise((resolve,reject)=>{
  if(!writer){reject(Error('USB disconnected'));return;}
  if(kind==='get'?pendingNodeGet:pendingNodeSet){reject(Error('Node key request already running'));return;}
  const slot={resolve,reject,timer:null};
  slot.timer=setTimeout(()=>{
   if(kind==='get'&&pendingNodeGet===slot)pendingNodeGet=null;
   if(kind==='set'&&pendingNodeSet===slot)pendingNodeSet=null;
   reject(Error('ESP32 did not respond to '+kind.toUpperCase()+' key command'));
  },5000);
  if(kind==='get')pendingNodeGet=slot;else pendingNodeSet=slot;
  writeLine(command).catch(err=>{
    clearTimeout(slot.timer);
    if(kind==='get'&&pendingNodeGet===slot)pendingNodeGet=null;
    if(kind==='set'&&pendingNodeSet===slot)pendingNodeSet=null;
    reject(err);
  });
 });
}
function finishNodeKeyRequest(kind,value,error){
 const slot=kind==='get'?pendingNodeGet:pendingNodeSet;
 if(!slot)return;
 if(kind==='get')pendingNodeGet=null;else pendingNodeSet=null;
 clearTimeout(slot.timer);
 if(error)slot.reject(error);else slot.resolve(value);
}
async function getNodeKey(){return await keyRequest('get','KEYGET');}
async function setNodeKey(raw){
 if(!(raw instanceof Uint8Array)||raw.length!==32)throw Error('Expected 32 key bytes');
 await keyRequest('set','KEYSET '+P.hex(raw));
}
/* Convert stored legacy browser-encrypted history to the node's key, without
   copying the key back into browser storage. Works for non-extractable keys. */
async function rewrapHistory(oldKey,newKey){
 const rows=await listStored();
 const replacements=[];
 for(const row of rows){
  const clear=await P.openAtRest(oldKey,row.seal,macKey(),row.id);
  const newSeal=await P.sealAtRest(newKey,clear,macKey(),row.id);
  replacements.push({...row,seal:newSeal});
 }
 for(const row of replacements)await dbPut('messages',row);
 return replacements.length;
}
async function adoptNodeKey(raw){
 const key=await P.importKey(raw);
 const legacy=await dbGet('keys',macKey());
 if(legacy?.key){
  const n=await rewrapHistory(legacy.key,key);
  await dbDelete('keys',macKey());
  if(n)toast(`Migrated ${n} browser-encrypted messages to the node key.`);
 }
 secretKey=key;rxGuard.clear();
 await migrateLegacy();
 await hydrateHistory();
 refreshStatus();sendReadReceipts();
}

function incomingKey(mac,session,transferId){return mac+':'+session+':'+transferId;}
function pruneIncoming(){const t=Date.now();for(const [k,v] of incoming){if(t-v.updated>4*60_000)incoming.delete(k);}rxGuard.prune();}
function flushSelectiveAck(key){
 const group=sackGroups.get(key);
 if(!group)return;
 sackGroups.delete(key);
 if(group.timer)clearTimeout(group.timer);
 if(!verified||!writer)return;
 const ids=[...group.ids];
 // The legacy APPACK command is the on-demand fallback if the batch USB
 // write fails. Sender's firmware also handles lost batch ACKs by retrying
 // the original data; duplicate RX then generates the old single ACK.
 writeLine(`APPACKB ${group.mac} ${group.session} ${ids.join(' ')}`).catch(()=>{
  if(writer)for(const id of ids)writeLine(`APPACK ${group.mac} ${group.session} ${id}`).catch(()=>{});
 });
}
function queueSelectiveAck(mac,session,packetId){
 const key=`${mac}:${session}`;
 let group=sackGroups.get(key);
 if(!group){
  group={mac,session,ids:new Set(),timer:null};
  sackGroups.set(key,group);
  group.timer=setTimeout(()=>flushSelectiveAck(key),SACK_FLUSH_MS);
 }
 group.ids.add(packetId);
 if(group.ids.size>=SACK_GROUP_MAX)flushSelectiveAck(key);
}
function clearSelectiveAcks(){
 for(const group of sackGroups.values())if(group.timer)clearTimeout(group.timer);
 sackGroups.clear();
}
async function processRx(mac,session,packetId,rawHex){
 let fr;
 try{
  if(!/^[0-9A-Fa-f]{40,456}$/.test(rawHex))return;
  fr=P.parseFrame(P.unhex(rawHex));
 }catch{return;}
 if(!fr||!secretKey||!verified)return;
 const key=incomingKey(mac,session,fr.transferId);
 /* A duplicate of an already decrypted transfer is safe to APPACK again.
  * Do NOT acknowledge unverified encrypted data to the application layer. */
 if(rxGuard.state(key)==='accepted'){
  void writeLine(`APPACK ${mac} ${session} ${packetId}`).catch(()=>{});
  return;
 }
 if(rxGuard.state(key)==='pending'){rxGuard.note(key,packetId);return;}
 let state=incoming.get(key);
 if(!state){
  if(incoming.size>=5){const oldest=incoming.keys().next().value;incoming.delete(oldest);}
  state={count:fr.count,total:fr.length,chunks:Array(fr.count),packetIds:new Set(),received:0,updated:Date.now()};
  incoming.set(key,state);
 }
 if(state.count!==fr.count||state.total!==fr.length)return;
 state.packetIds.add(packetId);
 if(!state.chunks[fr.index]){state.chunks[fr.index]=fr.bytes;state.received++;}
 state.updated=Date.now();
 if(state.received!==state.count)return;
 incoming.delete(key);
 const ticket=rxGuard.begin(key,state.packetIds);
 if(!ticket)return; // bounded: sender retries if all validation slots are busy
 const payload=P.concat(...state.chunks),keyAtStart=secretKey;
 if(payload.length!==state.total){rxGuard.reject(ticket);return;}
 try{
  const plain=await P.open(keyAtStart,payload,fr.transferId),env=P.parseEnvelope(plain);
  if(!verified||secretKey!==keyAtStart){rxGuard.reject(ticket);return;}
  if(![P.types.text,P.types.delivered,P.types.read,P.types.edit,
        P.types.delete,P.types.played,P.types.probe,P.types.probeAck,P.types.receiptBatch].includes(env.kind)){rxGuard.reject(ticket);return;}
  await handleEnvelope(env,mac);
  if(!verified||secretKey!==keyAtStart){rxGuard.reject(ticket);return;}
  rxGuard.accept(ticket);
  /* Hardware-level ACK was automatic. APPACK here records that the whole
   * message decrypted and was processed by this application. These are
   * local USB commands and do NOT trigger separate over-air packets. */
  for(const id of ticket.packetIds){
   void writeLine(`APPACK ${mac} ${session} ${id}`).catch(()=>{});
  }
 }catch(e){
  rxGuard.reject(ticket);
  console.warn('Encrypted message rejected:',e);
  toast('Encrypted transfer rejected: wrong pairing key, corrupted data, or authentication failure.');
 }
}
async function processRxBulk(mac,transferId,ciphertext){
 if(!verified||!secretKey||!(ciphertext instanceof Uint8Array))return;
 const key=`ARQ:${mac}:${transferId}`,ticket=rxGuard.begin(key);
 if(!ticket)return; // duplicate already processed/in progress, or guard at capacity
 const keyAtStart=secretKey;
 try{
  const plain=await P.open(keyAtStart,ciphertext,transferId);
  const env=P.parseEnvelope(plain);
  if(!verified||secretKey!==keyAtStart){rxGuard.reject(ticket);return;}
  await handleEnvelope(env,mac);
  if(!verified||secretKey!==keyAtStart){rxGuard.reject(ticket);return;}
  rxGuard.accept(ticket);
 }catch(e){
  rxGuard.reject(ticket);
  console.warn('Authenticated LoRa bulk rejected',e);
  toast('Encrypted LoRa transfer rejected: authentication or format failure');
 }
}
async function handleEnvelope(env,mac){
 if(env.kind===P.types.mediaPart){
  /* Every segment was authenticated by a distinct AES-GCM transaction.
   * No user-visible message or application delivered receipt until the
   * reassembled media also passes its full SHA-256 digest. */
  const result=await mediaReassembler.ingest(mac,env.id,env.body,
    {width:env.width,height:env.height,ts:env.ts});
  if(!result.complete)return;
  const code=result.kind==='image'?({'image/webp':0,'image/png':1,'image/jpeg':2})[result.mime]:
    ({'audio/webm':0,'audio/ogg':1,'audio/mp4':2})[result.mime];
  if(code===undefined)throw Error('Unsupported completed media codec');
  const reconstructed={kind:result.kind==='image'?P.types.image:P.types.voice,
    id:env.id,ts:result.metadata.ts,width:result.metadata.width,
    height:result.metadata.height,body:P.concat(Uint8Array.of(code),result.bytes)};
  await handleEnvelope(reconstructed,mac);
  return;
 }
 if(env.kind===P.types.receiptBatch){
  let items;
  try{items=globalThis.OffGridLoRaReceipts.unpack(env.body);}catch{return;}
  for(const item of items){
   const r=recordMap.get(item.targetId);
   if(r?.dir==='out')await setState(r,globalThis.OffGridLoRaDelivery.encryptedReceiptStatus(item.kind));
  }
  return;
 }
 if(env.kind===P.types.probe){
  /* Only a fully authenticated AES-GCM probe gets a browser-level ACK. */
  if(![64,256,512,1024].includes(env.body.length))return;
  enqueueReceipt('probeAck',env.id);
  return;
 }
 if(env.kind===P.types.probeAck){
  const test=speedDiagnostics.full;
  if(test&&env.body.length===8&&P.hex(env.body)===test.job.probeId){
   const job=test.job,ms=performance.now()-job.started;
   test.resolve({bytes:job.size,ms,frames:job.fragmentRTTs.length,
    avgFragmentMs:job.fragmentRTTs.reduce((a,b)=>a+b,0)/Math.max(1,job.fragmentRTTs.length)});
  }
  return;
 }
 if(env.kind===P.types.edit||env.kind===P.types.delete){
  if(env.body.length<8)return;
  const id=P.hex(env.body.subarray(0,8)),r=recordMap.get(id);
  if(!r||r.dir!=='in'||r.deleted||r.peerMac!==mac)return;
  if(env.kind===P.types.delete){
   if(env.body.length!==8)return;
   const audio=Array.from($('messages').children).find(x=>x.dataset.mid===id)?.querySelector('audio');
   if(audio){audio.pause();audio.removeAttribute('src');audio.load();}
   if(photoUrls.has(id)){URL.revokeObjectURL(photoUrls.get(id));photoUrls.delete(id);}
   if(voiceUrls.has(id)){URL.revokeObjectURL(voiceUrls.get(id));voiceUrls.delete(id);}
   r.deleted=true;r.text='This message was deleted';r.bytes=null;r.edited=false;
  }else{
   if(r.kind!=='text'||env.body.length>2112||env.body.length<9)return;
   const replacement=P.textFrom(env.body.subarray(8));
   if(!replacement.trim()||countChars(replacement)>MAX_CHARS)return;
   r.text=replacement;r.edited=true;
  }
  await storeRecord(r);refreshMessageBubble(r);updateSidebar();return;
 }
 if(env.kind===P.types.delivered||env.kind===P.types.read){if(env.body.length!==8)return;const original=P.hex(env.body),r=recordMap.get(original);if(r&&r.dir==='out')await setState(r,globalThis.OffGridLoRaDelivery.encryptedReceiptStatus(env.kind===P.types.read?'read':'delivered'));return;}
 if(env.kind===P.types.played){
  if(env.body.length!==8)return;
  const r=recordMap.get(P.hex(env.body));
  if(r?.dir==='out'&&r.kind==='voice'&&!r.played){
   r.played=true;updateBubble(r);
   try{await storeRecord(r);}catch(e){console.warn('Played receipt persistence failed',e);}
  }
  return;
 }
 if(env.kind!==P.types.text&&env.kind!==P.types.image&&env.kind!==P.types.voice)return;
 let r=recordMap.get(env.id);
 if(r){if(r.dir==='in'){
   logUsb(`RX_APP_DUPLICATE_ID|${r.id}|KIND=${r.kind}`);
   enqueueReceipt('delivered',r.id);if(r.readSent)enqueueReceipt('read',r.id);
   if(r.kind==='voice'&&r.played&&r.playedSent)enqueueReceipt('played',r.id);
  }return;}
 if(env.kind===P.types.text){let text;try{text=P.textFrom(env.body);}catch{return;}if(countChars(text)>MAX_CHARS)return;r={id:env.id,kind:'text',dir:'in',ts:env.ts||Date.now(),text,status:'received',readSent:false,deliveredAckSent:false,peerMac:mac};}
 else if(env.kind===P.types.image){if(env.body.length<2||env.body.length>Media.MAX_PHOTO+1)return;const mime=['image/webp','image/png','image/jpeg'][env.body[0]];if(!mime||!env.width||!env.height)return;r={id:env.id,kind:'image',dir:'in',ts:env.ts||Date.now(),mime,width:env.width,height:env.height,bytes:env.body.slice(1),status:'received',readSent:false,deliveredAckSent:false,peerMac:mac};}
 else{let voice;try{voice=P.unpackVoice(env.body,env.width+env.height*65536);}catch{return;}
  r={id:env.id,kind:'voice',dir:'in',ts:env.ts||Date.now(),mime:voice.mime,durationMs:voice.durationMs,bytes:voice.bytes,status:'received',readSent:false,deliveredAckSent:false,peerMac:mac};}
 peerMac=mac;
 await appendRecord(r);
 logUsb(`RX_APP_ACCEPTED|${r.id}|KIND=${r.kind}`);
 enqueueReceipt('delivered',r.id);
 if(isVisible())sendReadReceipts();else notifyIncoming(r);
}
const urgentReceipts=[];
const receiptPool=[];
let receiptBatchTimer=null;
let urgentRunning=false;
function flushReceiptBatch(){
 if(receiptBatchTimer!==null){clearTimeout(receiptBatchTimer);receiptBatchTimer=null;}
 while(receiptPool.length){
  const items=receiptPool.splice(0,globalThis.OffGridLoRaReceipts.MAX_ITEMS);
  urgentReceipts.push({kind:'receiptBatch',control:true,items,created:Date.now()});
 }
 void runUrgentReceipts();
}
function jobReceiptItems(job){return job.kind==='receiptBatch'?job.items:[{kind:job.kind,targetId:job.targetId}];}
async function runUrgentReceipts(){
 if(urgentRunning||!verified||!secretKey)return;
 urgentRunning=true;
 try{
  while(verified&&secretKey&&urgentReceipts.length){
   const job=urgentReceipts.shift();
   let done=false;
   /* A lost radio ACK for a receipt must not permanently strand a gray/blue
    * tick. A fresh AES-GCM envelope with the same target IDs is idempotent. */
   for(let attempt=0;attempt<2&&!done;attempt++){
    try{
     await transferFragments(await sealJob(job),job);
     done=true;
     for(const item of jobReceiptItems(job)){
      const r=recordMap.get(item.targetId);
      if(r&&r.dir==='in'){
       if(item.kind==='delivered')r.deliveredAckSent=true;
       if(item.kind==='read')r.readSent=true;
       await storeRecord(r);
      }
     }
    }catch(e){
     console.warn('Encrypted LoRa receipt unconfirmed',job.kind,attempt+1,e.message);
     if(attempt===0)await wait(80+Math.floor(Math.random()*150));
    }
   }
   for(const item of jobReceiptItems(job)){
    queuedReceipts.delete(item.kind+':'+item.targetId);
    if(!done&&item.kind==='read')pendingReads.delete(item.targetId);
   }
  }
 }finally{urgentRunning=false;}
}
function enqueueReceipt(kind,targetId){
 const key=kind+':'+targetId;
 if(queuedReceipts.has(key)||!secretKey)return;
 queuedReceipts.add(key);
 if(kind==='delivered'||kind==='read'){
  receiptPool.push({kind,targetId});
  if(receiptPool.length>=globalThis.OffGridLoRaReceipts.MAX_ITEMS)flushReceiptBatch();
  else if(receiptBatchTimer===null)receiptBatchTimer=setTimeout(flushReceiptBatch,65);
 }else{
  urgentReceipts.push({kind,control:true,targetId,created:Date.now()});
  void runUrgentReceipts();
 }
}

function sendReadReceipts(){
 if(!secretKey||!verified||!isVisible())return;
 for(const r of records){
  if(r.dir!=='in')continue;
  if(!r.seenLocally){
   r.seenLocally=true;
   void storeRecord(r).catch(e=>console.warn('Could not save local unread state:',e));
  }
  if(!r.readSent&&!pendingReads.has(r.id)){
   pendingReads.add(r.id);enqueueReceipt('read',r.id);
  }
 }
 updateUnreadBadge();
}
function makeJob(r){return {kind:r.kind,control:false,record:r,created:Date.now(),createdMono:performance.now()};}
async function sealJob(job){
 if(job.record&&['image','voice'].includes(job.record.kind))return {media:true};
 let transferId=P.newId();
 /* ARQ packet uses the complete 64-bit transfer ID (two LE32 words). */
 while(transferId.slice(0,8)==='00000000'||transferId.slice(8)==='00000000')transferId=P.newId();
 const id=job.control?P.newId():(job.kind==='probe'?job.probeId:job.record.id);
 let kind,body,width=0,height=0;
 if(job.control){
   kind=P.types[job.kind];body=job.kind==='receiptBatch'?
     globalThis.OffGridLoRaReceipts.pack(job.items):job.kind==='edit'?
     P.concat(P.unhex(job.targetId),P.textBytes(job.newText)):
     P.unhex(job.targetId);
  }
 else if(job.kind==='probe'){kind=P.types.probe;body=P.randomBytes(job.size);}
 else if(job.record.kind==='text'){kind=P.types.text;body=P.textBytes(job.record.text);}
 else if(job.record.kind==='voice'){
  kind=P.types.voice;body=P.packVoice(job.record.mime,job.record.bytes);
  /* Preserve the original 16-bit low word for 1.5 notes; use the previously
     unused metadata high word to encode durations up to 120000 ms. */
  width=job.record.durationMs&0xFFFF;height=Math.floor(job.record.durationMs/65536);
 }
 else{kind=P.types.image;const r=job.record,mimeNum=({'image/webp':0,'image/png':1,'image/jpeg':2})[r.mime];if(mimeNum===undefined)throw Error('Unsupported image encoding');body=P.concat(Uint8Array.of(mimeNum),r.bytes);width=r.width;height=r.height;}
 const plain=P.makeEnvelope(kind,id,body,Math.floor(Date.now()/1000),width,height);
 const ciphertext=await P.seal(secretKey,plain,transferId);
 if(ciphertext.length>globalThis.OffGridLoRaBulk.MAX)
   throw Error('Encrypted bulk transfer exceeds 7168 bytes');
 return {transferId,ciphertext};
}
async function oneFragment(frame,job,index){
 return rfScheduler.submit(job.control?'receipt':'data',async()=>{
 const hex=P.hex(frame),started=performance.now();
 await new Promise((resolve,reject)=>{
  if(radioByHex.has(hex)){reject(Error('Duplicate fragment already outstanding'));return;}
  const slot={hex,resolve,reject,wire:null,timer:null,job,index,sent:false};
  radioByHex.set(hex,slot);
  slot.timer=setTimeout(()=>{
   if(radioByHex.get(hex)===slot){
    clearSlot(slot);
    reject(Error('LoRa radio acknowledgment unconfirmed (peer may have received the frame)'));
   }
  },9000);
  writeLine((job.control?'CTRLHEX ':'MSGHEX ')+hex).catch(e=>{
   if(radioByHex.get(hex)===slot){clearSlot(slot);reject(e);}
  });
 });
 return {ms:Math.max(1,performance.now()-started),attempts:1};
 });
}
function onRadio(type,session,id,extra){
 const slot=radioByWire.get(`${session}:${id}`);
 if(!slot)return;
 if(type==='SENT'){
  if(!slot.sent){
   slot.sent=true;
   const job=slot.job;
   if(job?.record){
    if(globalThis.OffGridLoRaDelivery.markTxDone(job,slot.index))
     void setState(job.record,'sent');
   }
  }
  return;
 }
 if(type==='RADIOACK'||type==='DELIVERED'){
  /* DELIVERED accepted for old firmware compatibility. v1.1 uses RADIOACK;
   * neither event can generate double-gray or blue ticks. */
  clearSlot(slot);slot.resolve();
 }else if(type==='FAILED'){
  clearSlot(slot);slot.reject(Error(extra||'Radio acknowledgment missing'));
 }
}
/* A media RF change is a temporary, signed two-radio operation. The USB
 * write finishing does NOT mean the other ESP32 changed its modem. Wait for
 * MEDIA|READY after a genuine authenticated RF profile confirmation. */
async function mediaModeCommand(command,accepted,timeoutMs){
 if(mediaResponseWaiter)throw Error('Another media mode command is pending');
 let finish;
 const reply=new Promise((resolve,reject)=>{
  const timer=setTimeout(()=>{
   if(mediaResponseWaiter?.finish===finish){
    mediaResponseWaiter=null;reject(Error('Timed out waiting for synchronized media mode'));
   }
  },timeoutMs);
  finish=(line,error)=>{
   clearTimeout(timer);
   if(mediaResponseWaiter?.finish===finish)mediaResponseWaiter=null;
   if(error)reject(error);else resolve(line);
  };
  mediaResponseWaiter={accepted:new Set(accepted),finish};
 });
 try{
  // Attach a rejection handler before USB I/O so no timer rejection can leak.
  const [,line]=await Promise.all([writeLine(command),reply]);
  return line;
 }catch(e){
  if(mediaResponseWaiter?.finish===finish)finish(null,e);
  throw e;
 }
}
function onMediaLine(line){
 const parts=line.split('|'),kind=parts[1];
 if(kind==='RESTORE_FAILED')toast('Media RF restore failed; check both radio profiles before the next transfer');
 if(kind==='BYPASS')logUsb('MEDIA: Fast skipped: '+(parts[2]||'unknown'));
 if(mediaResponseWaiter?.accepted.has(kind))mediaResponseWaiter.finish(line);
}
async function beginMediaFastSession(){
 if(currentProfile==='ULTRA')throw Error('Ultra Long Range is encrypted text only. Select Max Range to send media.');
 // Do not start a media send while an unrelated manual/Auto profile change
 // is still being authenticated. A timeout leaves the chat retryable.
 if(profileTransition){
  const deadline=Date.now()+12000;
  while(profileTransition&&Date.now()<deadline)await wait(120);
  if(profileTransition)throw Error('A previous RF profile switch is still pending');
 }
 if(currentProfile!=='LOW'&&!highPaVerified){
  const permitted=confirm('For this photo or voice note, temporarily use Fast (+18 dBm) on BOTH LoRa boards and return to the previous mode afterward? I have verified both boards, 433 MHz antennas, stable 3.3 V under TX load, module limits and local radio permissions.');
  if(!permitted){logUsb('MEDIA: operator declined temporary high-power Fast');return false;}
  await writeLine('POWER18 VERIFIED');
 }
 const line=await mediaModeCommand('MEDIA ON',['READY','BYPASS','ERROR'],18000);
 if(line.startsWith('MEDIA|ERROR|'))throw Error('Media RF preflight: '+line.slice(12));
 if(line.startsWith('MEDIA|BYPASS|')){
  toast('Fast unavailable; sending media on the current reliable LoRa mode');
  return true;
 }
 if(line!=='MEDIA|READY|MODE=LOW')throw Error('Media Fast readiness was not confirmed');
 logUsb('MEDIA: synchronized Fast confirmed on the sender');
 return true;
}
async function endMediaFastSession(){
 if(!writer)return;
 try{
  const reply=await mediaModeCommand('MEDIA OFF',['RESTORED','OFF','RESTORE_FAILED'],20000);
  if(reply.startsWith('MEDIA|RESTORED|'))logUsb('MEDIA: previous synchronized radio mode restored');
 }catch(e){
  logUsb('MEDIA: restore unconfirmed: '+e.message);
  toast('Radio profile restoration unconfirmed; check both devices before sending again');
 }
}
/* Full encrypted OGM4 ciphertext staged over USB once. Firmware owns the
 * 224-byte RF fragments, burst window and authenticated bitmap retransmit.
 * Never acknowledge an application message merely for USB staging. */
/* Media reliability is layered over the unchanged v1.7 ARQ worker.
 * A 48 KiB photo becomes seven independent AES-GCM bulk transfers,
 * each with its own RF bitmap ACK, and is digest-checked at the recipient.
 * Retry only a failed segment; the receiver deduplicates identical parts.
 * Both ESP32s remain free to negotiate AUTO changes at segment boundaries. */
async function transferMedia(job){
 if(currentProfile==='ULTRA')throw Error('Ultra Long Range is text only; media transfer is disabled');
 const r=job.record;
 if(!r?.bytes)throw Error('No encrypted media bytes available');
 const pieces=await Media.split(r.kind,r.mime,r.bytes),w=r.kind==='voice'?r.durationMs&0xffff:r.width,
       h=r.kind==='voice'?Math.floor(r.durationMs/65536):r.height;
 const start=performance.now();let retries=0, outcome='OK';
 // End-to-end diagnostic counters contain sizes and timing only, NEVER
 // filenames, contents, ciphertext, keys or identifiers. No RF changes.
 const perf={preflight:0,restore:0,usb:0,rfWait:0,rfDevice:0,
             frames:0,retransmits:0,timeouts:0,segments:0};
 const previousMode=currentProfile;
 try{
  // Always request Fast for media, even if the user's normal mode is manual
  // Balanced/Range. The firmware may safely bypass on a weak or busy link.
  // Never send a single media fragment while a Fast switch is unconfirmed.
  const preflightStart=performance.now();
  try{await beginMediaFastSession();}
  finally{perf.preflight=performance.now()-preflightStart;}
  for(let i=0;i<pieces.length;i++){
   if(currentProfile==='ULTRA')throw Error('Auto entered Ultra; media paused. Retry after returning to Max Range.');
   if(r.cancelRequested)throw Error('Media transfer paused by user; tap to retry');
   if(!verified||!secretKey)throw Error('Disconnected; media is available to retry');
   const part=pieces[i];let complete=false,lastError;
   for(let attempt=0;attempt<3&&!complete;attempt++){
    if(profileTransition){
     const until=Date.now()+10000;
     while(profileTransition&&Date.now()<until)await wait(150);
     if(profileTransition)throw Error('RF mode negotiation did not finish');
    }
    let tid=P.newId();while(tid.slice(0,8)==='00000000'||tid.slice(8)==='00000000')tid=P.newId();
    const plain=P.makeEnvelope(P.types.mediaPart,r.id,part,Math.floor(r.ts/1000),w,h);
    const ciphertext=await P.seal(secretKey,plain,tid);
    if(ciphertext.length>globalThis.OffGridLoRaBulk.MAX)throw Error('Media segment exceeds radio bulk maximum');
    const sub={kind:'mediaPart',control:false,record:r,mediaSegment:true};
    try{
     await transferFragments({transferId:tid,ciphertext},sub);
     const m=sub.bulkMetrics||{};
     perf.usb+=m.usbStageMs||0;
     perf.rfWait+=m.radioWaitMs||0;
     perf.rfDevice+=m.RF_MS||0;
     perf.frames+=m.FRAMES||0;
     perf.retransmits+=m.RETX||0;
     perf.timeouts+=m.TIMEOUT||0;
     perf.segments++;
     logUsb(`MEDIASEG|${i+1}/${pieces.length}|BYTES=${part.length-28}|USB_MS=${Math.round(m.usbStageMs||0)}|RFWAIT_MS=${Math.round(m.radioWaitMs||0)}|RF_MS=${Math.round(m.RF_MS||0)}|FRAMES=${m.FRAMES??'?'}|RETX=${m.RETX??'?'}|TIMEOUT=${m.TIMEOUT??'?'}`);
     complete=true;
    }catch(e){
     lastError=e;retries++;
     if(!verified||!secretKey)break;
     if(attempt<2)await wait(350*(attempt+1));
    }
   }
   if(!complete)throw Error('Media part '+(i+1)+'/'+pieces.length+' not confirmed: '+(lastError?.message||'radio disconnected'));
   const percent=Math.round((i+1)/pieces.length*100);
   await setProgress(r,percent);
   /* A long image may take a minute. After this authenticated media segment
    * finishes, let ONE waiting text message through without interrupting an
    * RF frame, a bitmap ACK, or the signed media Fast session. */
   if(!profileTransition)await sendOneQueuedTextBetweenMediaSegments();
  }
  job.totalFrames=Math.ceil((r.bytes.length+pieces.length*80)/224);
 }catch(e){outcome='FAILED';throw e;}
 finally{
  // Await the signed return handshake before the next queued message starts.
  // Also clean up if encryption, cancellation or a radio segment failed.
  const restoreStart=performance.now();
  if(writer)await endMediaFastSession();
  perf.restore=performance.now()-restoreStart;
  const wall=performance.now()-start;
  const unaccounted=Math.max(0,wall-perf.preflight-perf.restore-perf.usb-perf.rfWait);
  job.mediaMetrics={durationMs:wall,segments:perf.segments,retries,
    preflightMs:perf.preflight,restoreMs:perf.restore,usbMs:perf.usb,
    radioWaitMs:perf.rfWait,deviceRfMs:perf.rfDevice,unaccountedMs:unaccounted};
  logUsb(`MEDIAPERF|${outcome}|FROM=${previousMode}|BYTES=${r.bytes.length}|SEG=${perf.segments}/${pieces.length}|WALL_MS=${Math.round(wall)}|PRE_MS=${Math.round(perf.preflight)}|USB_MS=${Math.round(perf.usb)}|RFWAIT_MS=${Math.round(perf.rfWait)}|RF_MS=${Math.round(perf.rfDevice)}|OTHER_MS=${Math.round(unaccounted)}|RESTORE_MS=${Math.round(perf.restore)}|FRAMES=${perf.frames}|RETX=${perf.retransmits}|TIMEOUT=${perf.timeouts}|RETRIES=${retries}`);
 }
}
async function transferFragments(packet,job){
 if(!bulkLink)throw Error('Bulk LoRa transport not connected');
 const started=performance.now(),frames=Math.ceil(packet.ciphertext.length/224);
 job.totalFrames=frames;job.transmitted=new Set();
 job.bulkMetrics=await rfScheduler.submit(job.control?'receipt':'data',()=>bulkLink.send(
    packet.transferId,packet.ciphertext,Boolean(job.control),{
      media:Boolean(job.mediaSegment),
      onStage:stage=>{
        // Stage names and sizes only. Never include encryption keys or text.
        if(!job.mediaSegment)logUsb(`BULK_STAGE|${job.kind}|${stage}|SIZE=${packet.ciphertext.length}`);
      },
      onSent:()=>{
        if(job.record&&!job.mediaSegment)void setState(job.record,'sent');
      },
      onProgress:(n,count)=>{
        if(job.record&&!job.mediaSegment){
          const progress=Math.round(100*n/count);
          const elapsed=Math.max(.001,(performance.now()-started)/1000);
          job.record.speedKbps=packet.ciphertext.length*(n/count)/1024/elapsed;
          void setProgress(job.record,progress);
        }
      }
    }));
 const elapsed=Math.max(1,performance.now()-started);
 recordLinkResult(elapsed,1,['image','voice'].includes(job.kind)&&!job.control);
 if(job.kind==='probe')job.fragmentRTTs.push(elapsed);
 if(job.record && ['image','voice'].includes(job.record.kind)&&!job.mediaSegment){
    linkStats.lastKBs=job.record.bytes.length/1024/(elapsed/1000);
    updateLinkBadge();
 }
}
/* Keep short texts responsive during photo/voice uploads. Uses the exact
 * same encrypt -> bulk ARQ -> authenticated-receipt rules as ordinary text;
 * only the queue turn order changes, at a COMPLETED media segment boundary. */
async function sendTextJob(job){
 const r=job.record;
 const started=performance.now();
 const queuedMs=Math.max(0,started-(job.createdMono||started));
 const stalled=setTimeout(()=>{
    // One bounded STATUS query permits distinguishing missing CAPS/RF ACK
    // from a stalled USB bulk stage without flooding the event queue.
    if(verified&&writer){
     logUsb('TEXT_STALL|NO_RF_CONFIRMATION_AFTER_10S|CHECK_BULK_STAGE_AND_BOTH_LINK_COUNTERS');
     void writeLine('STATUS').catch(()=>{});
    }
 },10000);
 try{
  const packet=await sealJob(job);
  await transferFragments(packet,job);
  await setState(r,'sent');
  weakTextRetry.holdForReceipt(r.id,Date.now(),currentProfile==='ULTRA');
  watchDeliveryReceipt(r,Math.ceil(packet.ciphertext.length/224));
  const m=job.bulkMetrics||{};
  logUsb(`TEXTPERF|CONFIRMED_RF|LOCAL_MS=${Math.round(r.localSaveMs||0)}|QUEUE_MS=${Math.round(queuedMs)}|USB_MS=${Math.round(m.usbStageMs||0)}|RFWAIT_MS=${Math.round(m.radioWaitMs||0)}|RF_MS=${Math.round(m.RF_MS||0)}|FRAMES=${m.FRAMES??'?'}|RETX=${m.RETX??'?'}|TIMEOUT=${m.TIMEOUT??'?'}`);
 }catch(e){
  weakTextRetry.postpone(r.id,Date.now());
  await setState(r,'waiting',e.message);
  logUsb('TEXT_FAILURE|'+String(e.message).slice(0,160));
  const now=Date.now();
  if(!sendTextJob.lastWeakNoticeAt || now-sendTextJob.lastWeakNoticeAt>12000){
   toast('Text saved encrypted. Retrying when the LoRa peer is usable.');
   sendTextJob.lastWeakNoticeAt=now;
  }
 }finally{clearTimeout(stalled);}
}
async function sendOneQueuedTextBetweenMediaSegments(){
 if(!verified||!secretKey||!bulkLink||profileTransition)return;
 const i=transferQueue.findIndex(j=>j.kind==='text'&&!j.control&&j.record);
 if(i<0)return;
 const [job]=transferQueue.splice(i,1);
 /* Leave activeTransfer pointing to the media parent: disconnect/cancel must
  * still identify that transfer until its restore handshake finishes. */
 await sendTextJob(job);
}
async function runTransfers(){
 if(runningTransfers||!verified||!secretKey)return;runningTransfers=true;
 try{
  while(verified&&secretKey&&transferQueue.length){
   // Restore the v3.0.4 proven FIFO dispatch on this hotfix branch.
   // The v3.0.5 priority reshuffle is disabled while we isolate a close-
   // range regression. Control receipts retain their separate urgent queue.
   const job=transferQueue.shift();
   activeTransfer=job;
   try{
    if(job.kind==='text'&&!job.control){await sendTextJob(job);continue;}
    const chunks=await sealJob(job);
    if(chunks.media)await transferMedia(job);
    else await transferFragments(chunks,job);
    if(job.control){
      const r=recordMap.get(job.targetId);if(r&&r.dir==='in'){
       if(job.kind==='read')r.readSent=true;
       if(job.kind==='delivered')r.deliveredAckSent=true;
       if(job.kind==='played')r.playedSent=true;
       await storeRecord(r);
      }
    }else if(job.record){
      /* All radio fragments were authenticated by the peer transceiver.
       * Only an encrypted delivered receipt can show the double tick. */
      await setState(job.record,'sent');
      watchDeliveryReceipt(job.record,chunks.media?(job.totalFrames||6):Math.ceil(chunks.ciphertext.length/224));
    }
   }catch(e){
    if(job.control){
      if(job.kind==='read')pendingReads.delete(job.targetId);
      if(['edit','delete'].includes(job.kind)){
       const r=recordMap.get(job.targetId);if(r){r.mutationFailed=true;refreshMessageBubble(r);await storeRecord(r);}
       toast(`Message ${job.kind} failed to sync. Reconnect and try again.`);
      }
      console.warn('Control send failed',job.kind,e.message);
     }
    else if(job.record){
      if(job.kind==='text'){
        weakTextRetry.postpone(job.record.id,Date.now());
        await setState(job.record,'waiting',e.message);
      }else{await setState(job.record,'failed',e.message);toast('Transfer failed. Click its bubble to retry.');}
    }
    else if(job.kind==='probe'&&speedDiagnostics.full?.job===job){speedDiagnostics.full.reject(e);}
   }finally{if(job.control)queuedReceipts.delete(job.kind+':'+job.targetId);activeTransfer=null;}
  }
 }finally{runningTransfers=false;}
}
function onProfileLine(line){
 const parts=line.split('|'),type=parts[1],mode=parts[2];
 if(type==='ACTIVE'||type==='ROLLED_BACK'){
   if(['LOW','BALANCED','MAX','ULTRA'].includes(mode))currentProfile=mode;
   refreshStatus();
   const values={};for(const field of parts.slice(3)){
     const i=field.indexOf('=');if(i>0)values[field.slice(0,i)]=field.slice(i+1);
   }
   const sf=Number(values.SF),bw=Number(values.BW),power=Number(values.TX_DBM);
   if(Number.isInteger(sf)&&sf>=7&&sf<=12)profileRadio.sf=sf;
   if([125000,250000,500000].includes(bw))profileRadio.bw=bw;
   if(Number.isInteger(power)&&power>=2&&power<=18)profileRadio.power=power;
   profileTransition=false;
   if(values.AUTO==='1'||values.AUTO==='0')autoMode=values.AUTO==='1';
   updateLinkBadge();
   if(type==='ROLLED_BACK')toast('Radio profile change rolled back; both nodes must be reachable.');
   else if(verified)toast(`LoRa profile active: ${profileLabels[currentProfile]}`);
 }else if(['NEGOTIATING','PEER_REQUEST','AGREED','VERIFYING','TRANSITION'].includes(type)){
   profileTransition=true;
   if(type==='NEGOTIATING')toast(`Negotiating ${profileLabels[mode]||mode} with the peer…`);
 }else if(type==='ERROR'||type==='TIMEOUT'){
   profileTransition=false;
   toast(`LoRa profile: ${parts.slice(2).join(' ')||'request failed'}`);
 }
}
function onAutoLine(line){
 const parts=line.split('|'),type=parts[1];
 if(type==='ON'||type==='PEER_ON'||type==='ACTIVE'||type==='OFF'||type==='PEER_OFF'){
   const fields={};for(const part of parts.slice(2)){const n=part.indexOf('=');if(n>0)fields[part.slice(0,n)]=part.slice(n+1);}
   if(type==='OFF'||type==='PEER_OFF')autoMode=false;
   else if(type==='ON'||type==='PEER_ON'||fields.ENABLED==='1')autoMode=true;
   autoBusy=false;updateLinkBadge();
   if(type==='ON')toast('Automatic radio adaptation enabled on both nodes');
   if(type==='OFF')toast('Automatic adaptation disabled; manual modes remain available');
 }else if(type==='NEGOTIATING'){
   autoBusy=true;
 }else if(type==='ERROR'){
   autoBusy=false;toast('Automatic LoRa: '+parts.slice(2).join(' '));
 }else if(type==='SELECT'){
   logUsb('AUTO selected '+parts[2]);
 }
}
/* Enabling AUTO asserts that the operator has checked the PA18 capability
 * of BOTH boards. The radio firmware's signed control packet coordinates the
 * peer; the peer does not need a browser click. */
async function requestAutoMode(enable){
 if(!verified||!writer||!secretKey){toast('Connect and pair the ESP32 first');return;}
 if(autoBusy||profileTransition){toast('Wait for the ongoing radio negotiation');return;}
 if(speedDiagnostics.busy||transferQueue.length||activeTransfer||pendingRadio()||
    rfScheduler.depth||urgentReceipts.length||bulkLink?.active){
   toast('Finish pending messages and tests before changing automatic mode');return;
 }
 if(enable && !confirm('Enable automatic Fast → Balanced → Range radio adaptation? I have verified BOTH radios, matched 433 MHz antennas, stable 3.3 V under nominal +18 dBm TX, and local operating rules. Automatic Balanced also uses nominal +18 dBm for link margin.'))return;
 autoBusy=true;
 try{
   if(enable){await writeLine('POWER18 VERIFIED');}
   await writeLine(`AUTO ${enable?'ON':'OFF'}`);
   toast(`Requesting authenticated automatic-mode ${enable?'activation':'deactivation'} on both radios…`);
 }catch(e){autoBusy=false;toast('Automatic mode request failed: '+e.message);}
}
/* Only one browser starts the change. The other follows a HMAC-authenticated
 * radio control request; the initiating user checks BOTH devices before PA18.
 * Never throw away normal outgoing text just to make a mode switch succeed. */
async function requestProfileChange(mode){
 if(!['LOW','BALANCED','MAX','ULTRA'].includes(mode))return;
 if(!verified||!writer||!secretKey){toast('Connect and pair the ESP32 first');return;}
 if(profileTransition){toast('A profile change is already in progress');return;}
 if(speedDiagnostics.busy){toast('Finish the speed test before changing modes');return;}
 const busy=Boolean(transferQueue.length||activeTransfer||pendingRadio()||
    rfScheduler.depth||urgentReceipts.length||bulkLink?.active);
 if(busy){toast('Wait until pending messages and receipts finish, then change mode');return;}
 if(mode===currentProfile&&!autoMode){toast(`Already using ${profileLabels[mode]}`);return;}
 const high=(mode==='LOW'||mode==='MAX'||mode==='ULTRA');
 if(high){
  if(!confirm('I have checked BOTH ESP32 radio boards, their matched 433 MHz antennas, stable 3.3 V supplies under TX load, hardware limits and local RF permissions. Authorize nominal +18 dBm on BOTH radios and coordinate the change with one click?'))return;
 }
 profileTransition=true;
 try{
  if(autoMode&&mode===currentProfile){await writeLine('AUTO OFF');profileTransition=false;return;} // Different manual mode: signed PROFILE OFFER disables Auto at both ends
  if(high&&!highPaVerified){
   // This command and the next are serialized on USB, and then by the
   // firmware command queue. The sender authorizes locally first.
   await writeLine('POWER18 VERIFIED');
  }
  await writeLine(`PROFILE ${mode}`);
  toast('Requesting synchronized profile change on both radios…');
 }catch(e){profileTransition=false;toast('Profile request failed: '+e.message);}
}
function showProfileDialog(){showDialog('Automatic + manual LoRa modes',body=>{
 body.append(el('p','','Automatic mode prioritizes Fast. As both directions weaken it steps to Balanced, then Range; it returns toward Fast after sustained improvement. Both ESP32s must run v1.8.0-rc.4. Mode changes use the proven signed one-browser handshake and rollback. There is no fixed distance threshold.'));
 const automatic=el('button','primary-button',autoMode?'Turn automatic mode OFF':'Enable automatic mode');
 automatic.disabled=!verified||autoBusy||profileTransition;
 automatic.onclick=()=>{closeDialog();void requestAutoMode(!autoMode);};body.append(automatic);
 body.append(el('p','',`Automatic: ${autoMode?'ON':'OFF'} · Manual selection overrides AUTO. Manual Balanced is +10 dBm; automatic Balanced uses nominal +18 dBm to avoid losing link budget when leaving Fast.`));
 const entries=[
  ['LOW','Fast / Nominal 18 dBm','SF7 · 500 kHz · +18 dBm (special PA_DAC; both boards must be checked)'],
  ['BALANCED','Balanced','SF7 · 250 kHz · +10 dBm · default at boot'],
  ['MAX','Max Power / Range','SF9 · 125 kHz · nominal +18 dBm; original range profile'],
  ['ULTRA','Ultra Long Range · TEXT ONLY','SF12 · 125 kHz · nominal +18 dBm; photos and voice disabled']
 ];
 for(const [value,name,details] of entries){
   const row=el('div','profile-mode-choice');
   const b=el('button','quiet-button',`${name}${currentProfile===value?' ✓':''}`);
   const note=el('p','',details);b.disabled=profileTransition||!verified;
   b.onclick=()=>{closeDialog();void requestProfileChange(value);};row.append(b,note);body.append(row);
 }
 body.append(el('p','','Switching waits for pending transfers and uses RSSI + SNR from BOTH authenticated radios with hysteresis. If negotiation fails, each radio attempts rollback. Reboot both radios to restore manual Balanced if settings diverge. No +22 dBm mode.'));
 const diag=el('button','quiet-button','USB diagnostics');diag.onclick=diagnoseUsb;body.append(diag);
});}
function parseLine(raw){const s=raw.trim();if(!s)return;
 if(diagnosticLine(s))return;
 // Record serial input for diagnosis, without exposing the 256-bit pairing key.
 usbLastRawAt=performance.now();
 if(s.startsWith('KEYDATA|'))logUsb('ESP32> KEYDATA|<redacted>');
 else if(s.startsWith('LINK|'))logUsb('ESP32> '+compactLinkDiagnostic(s));
 else logUsb('ESP32> '+s);
 if(s.startsWith('READY|'))usbSawBeacon=true;
 if(s.startsWith('FATAL|')){showUsbHint('ESP32 startup error: '+s.slice(6));return;}
 if(s.startsWith('KEYDATA|')){
  const h=s.slice(8);
  try{if(!/^[A-Fa-f0-9]{64}$/.test(h))throw Error('Invalid key response');finishNodeKeyRequest('get',P.unhex(h));}
  catch(e){finishNodeKeyRequest('get',null,e);}
  return;
 }
 if(s==='KEYEMPTY'){finishNodeKeyRequest('get',null);return;}
 if(s==='KEYSTORED'){finishNodeKeyRequest('set',true);return;}
 if(s.startsWith('KEYERROR|')){finishNodeKeyRequest('set',null,Error(s.slice(9)));return;}

 if(s.startsWith('POWER18|')){
  highPaVerified=s.startsWith('POWER18|ENABLED');
  toast(highPaVerified?'High-PA authorization accepted on the initiating node':'High-PA authorization disabled or refused');return;
 }
 if(bulkLink?.onLine(s))return;
 if(s.startsWith('MEDIA|')){onMediaLine(s);return;}
 if(s.startsWith('PROFILE|')){onProfileLine(s);return;}
 if(s.startsWith('AUTO|')){onAutoLine(s);return;}
 if(s.startsWith('LINK|')){
  const parts=s.split('|'),fields={};for(const bit of parts.slice(2)){
   const j=bit.indexOf('=');if(j>0)fields[bit.slice(0,j)]=bit.slice(j+1);
  }
  const rssi=Number(fields.RSSI);
  const age=Number(fields.PEER_AGE_MS),heard=fields.RFHEARD==='1'&&
    Number.isFinite(age)&&age>=0&&age<30000&&parts[1]!=='NONE';
  peerIsFresh=heard;freshLoRaAt=heard?Date.now():0;
  radioTelemetry={peer:parts[1],phy:0,mode:'LORA',rssi:heard&&Number.isFinite(rssi)&&rssi>=-160&&rssi<0?rssi:null,
    delivered:Number(fields.OK)||0,retries:Number(fields.RETRY)||0,failures:Number(fields.FAIL)||0,
    txok:Number(fields.TXOK)||0,txfail:Number(fields.TXFAIL)||0,rx:Number(fields.RX)||0,
    hello:Number(fields.HELLO)||0,healthGood:0,pingOk:0,pingLost:0,available:true};
  updateLinkBadge();return;
 }
 if(s.startsWith('RATE_ERROR|')){console.warn('Radio PHY configuration:',s);toast('ESP32 PHY rate could not be applied; falling back to safe rate.');return;}
 if(s.startsWith('RATE|')||s.startsWith('PEER|'))return;
 if(s.startsWith('READY|')){const parts=s.split('|');speedDiagnostics.capable=parts.includes('DIAG=1');speedDiagnostics.sackCapable=parts.includes('SACK=1');if(!parts.includes('MEDIAFAST=1')||!parts.includes('RF_POLICY=ULTRA12_FEC8')||!parts.includes('ULTRA=SF12_TEXT_ONLY')||!parts.includes('FW=1.8.0-rc.4')||!parts.includes('PROTO=2')||!parts.includes('CAP=180')||!parts.includes('STABLE=1')||!parts.includes('NVSKEY=1')||!parts.includes('ACKENGINE=2')||!parts.includes('FLOW=2')||!parts.includes('PROFILES=4')||!parts.includes('SYNC=ONE_CLICK')||!parts.includes('AUTORF=1')||!parts.includes('ARQ=1')||!parts.includes('BULK=1')||!parts.includes('USBPIPE=4')||!parts.includes('MEDIA=1')){if(!firmwareMismatchNotified){toast('Wrong ESP32 firmware protocol; open Diagnostics to see its READY response.');firmwareMismatchNotified=true;}showUsbHint('Expected LoRa v1.8.0-rc.4 firmware on BOTH ESP32s: FLOW=2, ACKENGINE=2, PROTO=2, FAMILY=LORA.');console.warn('Incompatible firmware READY:',s);return;}const mac=parts[1];if(!/^[0-9A-Fa-f:]{17}$/.test(mac))return;if(!parts.includes('FAMILY=LORA')){toast('Wrong firmware: connect an OffGrid Messenger LoRa node.');return;}if(verified&&localMac===mac)return;void onReady(mac);return;}
 if(s.startsWith('QUEUED|')){const [,session,id,hex]=s.split('|'),slot=radioByHex.get(hex);if(slot){slot.wire=`${session}:${id}`;radioByWire.set(slot.wire,slot);}return;}
 if(s.startsWith('RX|')){const [,mac,session,id,data]=s.split('|');if(mac&&session&&id&&data)void processRx(mac,session,id,data);return;}
 if(/^(SENT|RADIOACK|DELIVERED|FAILED|RETRY|RADIO_ERROR)\|/.test(s)){const [type,session,id,extra]=s.split('|');if(type==='RADIO_ERROR'){console.warn('LoRa:',extra);if(radioTelemetry.txfail<=2)toast('Radio send issue: '+extra+' — check Radio diagnostics in Settings.');}else onRadio(type,session,id,extra);return;}
 if(s.startsWith('ERROR|')){const reason=s.slice(6);
  if(reason==='EVENT_QUEUE_FULL' && !verified && handshakeTimer){
   clearInterval(handshakeTimer);handshakeTimer=null;
   showUsbHint('ESP32 command queue stalled. Stop STATUS retries; inspect the radio task boot log, SX1278 SPI/power or reset the board.');
  }if(pendingRadio()&&/^(QUEUE_FULL|EVENT_QUEUE_FULL|PROFILE_SWITCH_IN_PROGRESS|INVALID_HEX_OR_LENGTH|LINE_TOO_LONG|UNKNOWN_COMMAND)/.test(reason)){const slot=[...radioByHex.values()].findLast(x=>!x.wire)||[...radioByHex.values()].at(-1);if(slot){clearSlot(slot);slot.reject(Error('ESP32: '+reason));}}toast('ESP32: '+reason);}
}
function consume(data){serialBuffer+=new TextDecoder().decode(data);let i;while((i=serialBuffer.indexOf('\n'))>=0){parseLine(serialBuffer.slice(0,i));serialBuffer=serialBuffer.slice(i+1);}if(serialBuffer.length>8192)serialBuffer='';}
async function readLoop(){try{while(port?.readable&&!closing){reader=port.readable.getReader();try{while(!closing){const {value,done}=await reader.read();if(done)break;if(value)consume(value);}}finally{reader.releaseLock();reader=null;}if(!closing&&port?.readable)await wait(100);}}catch(e){if(!closing)toast('Serial disconnected: '+e.message);}finally{if(!closing){readPromise=null;void disconnect();}}}
async function onReady(mac){
 bulkLink=new globalThis.OffGridLoRaBulk.BulkLink(writeLine,processRxBulk);
 if(rfScheduler.closed)rfScheduler=new globalThis.OffGridLoRaScheduler.FragmentScheduler(64);clearSelectiveAcks();firmwareMismatchNotified=false;localMac=mac.toUpperCase();verified=true;
 if(usbWaitTimer){clearInterval(usbWaitTimer);usbWaitTimer=null;}
 clearUsbHint();
 // LoRa v1.0 has a fixed hardware radio profile; no RATE command exists.
 if(handshakeTimer){clearInterval(handshakeTimer);handshakeTimer=null;}
 try{
   try{setName(localStorage.getItem(nameKey())||localStorage.getItem('offgrid_phase13_name_'+macKey().replace(/:/g,'-'))||'Nearby node');}catch{setName('Nearby node');}
   const raw=await getNodeKey();
   if(raw){
     try{keyFingerprint=await radioKeyFingerprint(raw);logUsb('PAIRING_KEY_FP|SHA256_48='+keyFingerprint);}
     catch(e){keyFingerprint=null;logUsb('PAIRING_KEY_FP|UNAVAILABLE');}
     await adoptNodeKey(raw);
     void updateContactAvatar();
     for(const r of records){
      if(r.dir!=='in')continue;
      if(!r.deliveredAckSent)enqueueReceipt('delivered',r.id);
      if(r.kind==='voice'&&r.played&&!r.playedSent)enqueueReceipt('played',r.id);
     }
     sendReadReceipts();startWeakRetry();
   }else{
     keyFingerprint=null;secretKey=null;records=[];recordMap.clear();renderHistory();
     refreshStatus();showPairing();
   }
   refreshStatus();
 }catch(e){toast('Node pairing initialization failed: '+e.message);console.error(e);refreshStatus();}
}
async function connect(){if(!navigator.serial){toast('Use Chromium with Web Serial on localhost or HTTPS.');return;}try{
 if(!db)db=await openDb();port=await navigator.serial.requestPort();await port.open({baudRate:115200});
    usbLog.length=0;usbHandshakeIssue='';usbSawBeacon=false;usbHandshakeStarted=performance.now();usbLastRawAt=0;clearUsbHint();
    writer=port.writable.getWriter();refreshStatus();readPromise=readLoop();
    handshakeAttempts=1;logUsb('HOST> STATUS');await writeLine('STATUS');
    // Do not flood a stalled radio-worker's 32-event command queue.
    handshakeTimer=setInterval(()=>{
      if(!verified&&writer&&handshakeAttempts<6){
        handshakeAttempts++;logUsb('HOST> STATUS (bounded retry)');
        writeLine('STATUS').catch(()=>{});
      } else if(handshakeAttempts>=6){clearInterval(handshakeTimer);handshakeTimer=null;}
    },5000);
    usbWaitTimer=setInterval(updateUsbWait,1200);
 }catch(e){toast('USB connection failed: '+e.message);await disconnect();}}
async function disconnect(){if(closing)return;closing=true;stopVoiceRecording(true);
 speedDiagnostics.serial?.reject(Error('USB disconnected during benchmark'));
 speedDiagnostics.full?.reject(Error('USB disconnected during benchmark'));
 rfScheduler.close('USB disconnected');
 bulkLink?.cancel('USB disconnected');bulkLink=null;
 speedDiagnostics.capable=false;speedDiagnostics.sackCapable=false;clearSelectiveAcks();
 try{if(handshakeTimer)clearInterval(handshakeTimer);handshakeTimer=null;if(usbWaitTimer){clearInterval(usbWaitTimer);usbWaitTimer=null;}if(reader)try{await reader.cancel();}catch{}if(readPromise)try{await readPromise;}catch{}if(writer){try{writer.releaseLock();}catch{}writer=null;}if(port){try{await port.close();}catch{}port=null;}}
 finally{
  if(mediaResponseWaiter)mediaResponseWaiter.finish(null,Error('Disconnected'));
  finishNodeKeyRequest('get',null,Error('Disconnected'));finishNodeKeyRequest('set',null,Error('Disconnected'));
  clearWeakRetry();keyFingerprint=null;
 readPromise=null;serialBuffer='';verified=false;port=null;writer=null;clearUsbHint();if(pendingRadio())failPendingRadio(Error('Disconnected'));if(activeTransfer?.record){const r=activeTransfer.record;
     void setState(r,r.kind==='text'?'waiting':'failed','Disconnected');}
 // Persist unsent text. Never silently discard it with an in-memory queue.
 for(const job of transferQueue){
   if(job.record&&job.kind==='text'&&job.record.dir==='out')
     void setState(job.record,'waiting','USB disconnected; saved encrypted');
 }
 transferQueue=[];runningTransfers=false;
  if(receiptBatchTimer!==null){clearTimeout(receiptBatchTimer);receiptBatchTimer=null;}
  receiptPool.length=0;urgentReceipts.length=0;queuedReceipts.clear();pendingReads.clear();incoming.clear();rxGuard.clear();mediaReassembler.clear();localMac=null;secretKey=null;radioTelemetry.available=false;profileTransition=false;autoBusy=false;autoMode=false;highPaVerified=false;currentProfile='BALANCED';profileRadio={sf:7,bw:250000,power:10};refreshStatus();closing=false;}}
/* A browser can deliver rapid click + Enter events while IndexedDB is
 * asynchronously storing a message. Lock and clear synchronously BEFORE the
 * first await; otherwise two independent IDs are created for one "hi". */
let sendTextBusy=false;
async function sendText(){
 if(sendTextBusy)return;
 if(profileTransition){toast('Wait for the LoRa profile switch to finish');return;}
 const input=$('messageInput'),text=input.value.trim();if(!verified||!secretKey||!text)return;
 if(countChars(text)>MAX_CHARS){toast('Maximum 500 characters per text message.');return;}
 sendTextBusy=true;
 input.value='';updateByteCount();
 try{
  const r={id:P.newId(),kind:'text',dir:'out',ts:Date.now(),text,status:'sending',progress:0,textClickedAt:performance.now()};
  await appendRecord(r);r.localSaveMs=performance.now()-r.textClickedAt;
  weakTextRetry.attempt(r.id,Date.now());
  if(transferQueue.length>=MAX_TEXT_QUEUE){
   await setState(r,'failed','Local text queue is full; retry once it drains');
   toast('LoRa text queue full; let pending messages finish');return;
  }
  transferQueue.push(makeJob(r));void runTransfers();
 }catch(e){
  /* A failed encrypted local save is not a successfully queued message. */
  input.value=text;updateByteCount();
  toast('Message could not be queued locally: '+String(e?.message||e).slice(0,100));
 }finally{sendTextBusy=false;}
}
function updatePrimaryAction(){const composer=$('composer'),input=$('messageInput'),voice=$('voiceButton'),send=$('sendButton');if(!composer||!input||!voice||!send)return;const hasText=!!input.value.trim();const recording=!!currentVoiceRecording;composer.classList.toggle('show-send',!recording&&hasText);composer.classList.toggle('show-voice',recording||!hasText);composer.classList.toggle('recording-mode',recording);voice.setAttribute('aria-hidden',(!recording&&hasText)?'true':'false');send.setAttribute('aria-hidden',(recording||!hasText)?'true':'false');}
function updateByteCount(){const input=$('messageInput'),n=countChars(input.value);$('byteCount').textContent=`${n}/${MAX_CHARS}`;$('byteCount').classList.toggle('over',n>MAX_CHARS);input.style.height='36px';input.style.height=Math.min(120,input.scrollHeight)+'px';$('sendButton').disabled=!verified||!secretKey||!input.value.trim()||n>MAX_CHARS;updatePrimaryAction();}
function convertCanvas(canvas,type,q){return new Promise(resolve=>canvas.toBlob(resolve,type,q));}
async function preparePhoto(file){if(!file||!file.type.startsWith('image/'))throw Error('Select a picture.');if(file.size>25*1024*1024)throw Error('Original photo is too large to process (25 MB maximum).');
 let bitmap;try{bitmap=await createImageBitmap(file);}catch{throw Error('Your browser cannot decode this photo format. Try JPEG or PNG.');}
 try{
  const originalW=bitmap.width,originalH=bitmap.height;if(!originalW||!originalH)throw Error('Invalid image dimensions');
  if(originalW*originalH>40_000_000)throw Error('Image resolution too large to process safely.');const canvas=document.createElement('canvas'),ctx=canvas.getContext('2d',{alpha:true});if(!ctx)throw Error('Canvas unavailable');
  if(imageMode==='lossless'){
   if(originalW*originalH>20_000_000)throw Error('Lossless photo is too large. Choose High quality in Settings.');
   canvas.width=originalW;canvas.height=originalH;ctx.drawImage(bitmap,0,0);
   const blob=await convertCanvas(canvas,'image/png');if(!blob||blob.size>MAX_PHOTO)throw Error('Lossless PNG exceeds 48 KiB; use High quality WebP or JPEG.');
   return {bytes:new Uint8Array(await blob.arrayBuffer()),mime:'image/png',width:originalW,height:originalH};
  }
  let candidate=null;
  const targetBytes=photoBudget();
  for(const maxEdge of [1600,1440,1280,1152,1024,960,900,820,760,700,640]){
   const scale=Math.min(1,maxEdge/Math.max(originalW,originalH));const w=Math.max(1,Math.round(originalW*scale)),h=Math.max(1,Math.round(originalH*scale));
   canvas.width=w;canvas.height=h;ctx.fillStyle='#ffffff';ctx.fillRect(0,0,w,h);ctx.drawImage(bitmap,0,0,w,h);
   for(const q of [0.91,0.87,0.83,0.79,0.75,0.71,0.67]){
    let blob=await convertCanvas(canvas,'image/webp',q);
    if(!blob||blob.type!=='image/webp')blob=await convertCanvas(canvas,'image/jpeg',q);
    if(!blob)throw Error('Cannot encode photo');
    if(blob.size<=targetBytes){candidate={bytes:new Uint8Array(await blob.arrayBuffer()),mime:blob.type,width:w,height:h};break;}
   }if(candidate)break;
  }
  if(!candidate)throw Error('Could not preserve enough detail within 48 KiB. Choose a less detailed photo or crop it first.');
  return candidate;
 }finally{bitmap.close?.();}
}
async function sendPhoto(file){if(currentProfile==='ULTRA'){toast('Ultra SF12 is text only. Photos are disabled.');return;}if(!verified||!secretKey)return;toast('Preparing photo and stripping metadata…');
 try{const photo=await preparePhoto(file);if(!verified||!secretKey)throw Error('Disconnected while preparing image');
 if(transferQueue.length>=MAX_TEXT_QUEUE)throw Error('Media queue full');
 const r={id:P.newId(),kind:'image',dir:'out',ts:Date.now(),mime:photo.mime,width:photo.width,height:photo.height,bytes:photo.bytes,status:'sending',progress:0};
 await appendRecord(r);transferQueue.push(makeJob(r));runTransfers();toast(`Photo prepared: ${photo.width}×${photo.height}, ${formatBytes(photo.bytes.length)}. Encrypted transfer queued.`);
 }catch(e){toast('Photo not sent: '+e.message);}finally{$('photoPicker').value='';}}

/* Browser microphone only. Capture/Opus encode on the laptop, AES-GCM encrypt
 * BEFORE any bytes reach the ESP32. The firmware handles exactly the existing
 * transport frames (no microphone permission or audio codec code on the MCU).
 * No remote uploads, no background microphone recording, no autoplay. */
function voiceButtonState(){
 const btn=$('voiceButton');if(!btn)return;
 const active=!!currentVoiceRecording;
 btn.classList.toggle('recording',active);
 btn.setAttribute('aria-pressed',String(active));
 btn.setAttribute('aria-label',active?'Stop and send voice message':'Record voice message');
 btn.title=active?'Stop and send voice note':'Record encrypted voice note (40 KiB max; actual duration depends on compression)';
 if(active){
  const elapsed=Math.min(MAX_VOICE_MS,Date.now()-currentVoiceRecording.started);
  $('composerStatus').textContent=`🔴 Recording voice · ${Math.floor(elapsed/1000)}s · ${formatBytes(currentVoiceRecording.capturedBytes)} / 40 KiB · tap mic to send`;
 }else if(verified&&secretKey){
  $('composerStatus').textContent=currentProfile==='ULTRA'?'Ultra SF12 · encrypted TEXT ONLY':'Encrypted media · LoRa v1.8';
 }
 updatePrimaryAction();
}
function stopVoiceRecording(cancel=false){
 const v=currentVoiceRecording;if(!v)return;
 if(cancel)v.cancelled=true;
 if(v.recorder.state!=='inactive'){
  try{v.recorder.stop();}catch(e){console.warn('Voice recorder stop:',e);}
 }
}
async function toggleVoiceRecording(){
 if(currentProfile==='ULTRA'&&!currentVoiceRecording){toast('Ultra SF12 is text only. Voice notes are disabled.');return;}
 if(currentVoiceRecording){stopVoiceRecording();return;}
 if(!verified||!secretKey){toast('Connect and pair both ESP32s before recording.');return;}
 if(!navigator.mediaDevices?.getUserMedia||!globalThis.MediaRecorder){toast('Microphone recording requires Chromium on localhost or HTTPS.');return;}
 const candidate=['audio/webm;codecs=opus','audio/webm','audio/ogg;codecs=opus','audio/mp4'];
 const mime=candidate.find(type=>MediaRecorder.isTypeSupported(type));
 if(!mime){toast('No supported microphone audio format in this browser.');return;}
 let stream;
 try{
  stream=await navigator.mediaDevices.getUserMedia({audio:{channelCount:{ideal:1},echoCancellation:true,noiseSuppression:true,autoGainControl:true},video:false});
  if(!verified||!secretKey){stream.getTracks().forEach(track=>track.stop());return;}
  const recorder=new MediaRecorder(stream,{mimeType:mime,audioBitsPerSecond:8000});
  const v={recorder,stream,chunks:[],capturedBytes:0,started:Date.now(),cancelled:false,mime};
  currentVoiceRecording=v;
  recorder.ondataavailable=event=>{if(event.data?.size){v.chunks.push(event.data);v.capturedBytes+=event.data.size;if(v.capturedBytes>=Media.MAX_VOICE-3*1024&&recorder.state!=='inactive')stopVoiceRecording();}};
  recorder.onerror=event=>{console.warn('MediaRecorder error',event.error);v.cancelled=true;stopVoiceRecording(true);toast('Voice recording failed.');};
  recorder.onstop=async()=>{
   const durationMs=Math.max(100,Math.min(MAX_VOICE_MS,Date.now()-v.started));
   stream.getTracks().forEach(track=>track.stop());
   if(voiceTimer){clearInterval(voiceTimer);voiceTimer=null;}
   if(currentVoiceRecording===v)currentVoiceRecording=null;
   voiceButtonState();refreshStatus();
   if(v.cancelled)return;
   try{
    const blob=new Blob(v.chunks,{type:recorder.mimeType||mime});
    if(blob.size>Media.MAX_VOICE)throw Error('Voice note exceeded 40 KiB. This browser could not compress the full recording to the limit; try a shorter note.');
    if(blob.size<256)throw Error('Recording is empty or too short.');
    if(!verified||!secretKey)throw Error('ESP32 disconnected; recording discarded.');
    const bytes=new Uint8Array(await blob.arrayBuffer());
    const codec=P.voiceCodecForMime(blob.type);
    if(!codec)throw Error('Unsupported audio codec');
    if(transferQueue.length>=MAX_TEXT_QUEUE)throw Error('Media queue full');
    const r={id:P.newId(),kind:'voice',dir:'out',ts:Date.now(),mime:codec.mime,durationMs,bytes,status:'sending',progress:0};
    await appendRecord(r);transferQueue.push(makeJob(r));runTransfers();
    toast(`Encrypted voice note queued · ${Math.round(durationMs/1000)}s · ${formatBytes(bytes.length)}.`);
   }catch(error){toast('Voice not sent: '+error.message);}
  };
  recorder.start(250);
  voiceButtonState();
  voiceTimer=setInterval(()=>{
    if(currentVoiceRecording!==v)return;
    voiceButtonState();
    if(Date.now()-v.started>=MAX_VOICE_MS)stopVoiceRecording();
  },240);
 }catch(e){
  stream?.getTracks().forEach(track=>track.stop());
  currentVoiceRecording=null;voiceButtonState();
  toast('Microphone unavailable: '+e.message);
 }
}

function refreshMessageBubble(r){
 const old=Array.from($('messages').children).find(x=>x.dataset.mid===r.id);
 if(!old)return;
 makeBubble(r,false);
 const fresh=$('messages').lastElementChild;
 old.replaceWith(fresh);
}
function queueMutation(kind,r,newText=''){
 if(!verified||!secretKey){toast('Connect your ESP32 before editing or deleting for everyone.');return false;}
 transferQueue.unshift({kind,control:true,targetId:r.id,newText,created:Date.now()});
 runTransfers();return true;
}
async function deleteLocalRecord(r){
 recordMap.delete(r.id);records=records.filter(x=>x.id!==r.id);
 await dbDelete('messages',macKey()+'/'+r.id);
 renderHistory();toast('Deleted from this browser only');closeDialog();
}
function showMessageActions(id){
 const r=recordMap.get(id);if(!r)return;
 showDialog('Message actions',body=>{
  const actions=el('div','message-actions');
  const add=(label,handler)=>{const button=el('button','quiet-button',label);
   button.type='button';button.onclick=handler;actions.append(button);};
  if(r.kind==='text'&&!r.deleted)add('Copy text',()=>{navigator.clipboard?.writeText(r.text).then(()=>toast('Copied')).catch(()=>toast('Clipboard unavailable'));closeDialog();});
  if(r.dir==='out'&&r.kind==='text'&&!r.deleted&&verified&&['delivered','read'].includes(r.status))add('Edit message',()=>{
   showDialog('Edit message',container=>{
    const field=document.createElement('textarea');field.value=r.text;field.maxLength=2000;field.rows=4;container.append(field);
    const save=el('button','primary-button','Save edit');save.onclick=async()=>{
     const replacement=field.value.trim();if(!replacement||countChars(replacement)>MAX_CHARS){toast('Enter 1–500 characters');return;}
     if(!queueMutation('edit',r,replacement))return;
     r.text=replacement;r.edited=true;r.mutationFailed=false;await storeRecord(r);refreshMessageBubble(r);updateSidebar();closeDialog();
    };container.append(save);field.focus();
   });
  });
  if(r.dir==='out'&&['image','voice'].includes(r.kind)&&r.status==='sending')
   add('Pause media transfer',async()=>{
     r.cancelRequested=true;
     if(activeTransfer?.record!==r){
       transferQueue=transferQueue.filter(j=>j.record!==r);
       await setState(r,'failed','Paused; click message to retry');
     }else toast('Media transfer pauses after the current encrypted segment');
     closeDialog();
   });
  add('Delete for me',()=>{if(confirm('Delete only from this browser?'))void deleteLocalRecord(r).catch(e=>toast(e.message));});
  if(r.dir==='out'&&!r.deleted&&verified&&['delivered','read'].includes(r.status))add('Delete for everyone',async()=>{
   if(!confirm('Delete this message on updated OffGrid clients too? An offline or older client may keep its existing copy.'))return;
   if(!queueMutation('delete',r))return;
   r.deleted=true;r.bytes=null;r.text='You deleted this message';r.edited=false;r.mutationFailed=false;
   await storeRecord(r);refreshMessageBubble(r);updateSidebar();closeDialog();
  });
  body.append(el('p','', 'Long press a bubble on mobile, or right-click it on desktop. Edits and deletions use encrypted OGM4 controls.'));
  body.append(actions);
 });
}
function retryMessage(id){const r=recordMap.get(id);if(!r||r.dir!=='out'||!['failed','waiting'].includes(r.status)||!verified||!secretKey)return;
 if(transferQueue.some(j=>j.record?.id===id)||activeTransfer?.record?.id===id)return;
 if(r.kind==='text')weakTextRetry.attempt(id,Date.now());r.status='sending';r.progress=0;r.error='';r.cancelRequested=false;updateBubble(r);void storeStatus(r);transferQueue.push(makeJob(r));runTransfers();}
function showDialog(title,render){$('modalTitle').textContent=title;const content=$('modalBody');content.replaceChildren();render(content);$('modalBackdrop').classList.remove('hidden');}
function closeDialog(){$('modalBackdrop').classList.add('hidden');}
function el(tag,cl='',text=''){const out=document.createElement(tag);if(cl)out.className=cl;out.textContent=text;return out;}
async function installPairKey(raw){
 if(!verified||!localMac)throw Error('Connect a node first');
 if(runningTransfers||pendingRadio()||transferQueue.length||rxGuard.pending.size)throw Error('Finish active transfers and incoming authentication before changing keys');
 if(secretKey&&!confirm('Replace the key stored on this ESP32? Both ESP32 nodes must use the SAME key. Encrypted history will be migrated locally when possible.'))return false;
 const key=await P.importKey(raw);
 /* Verify that existing history can be decrypted before changing the node key. */
 const current=secretKey||(await dbGet('keys',macKey()))?.key||null;
 let rows=[];
 if(current){rows=await listStored();for(const row of rows)await P.openAtRest(current,row.seal,macKey(),row.id);}
 await setNodeKey(raw);
 if(current&&rows.length)await rewrapHistory(current,key);
 await dbDelete('keys',macKey());
 secretKey=key;rxGuard.clear();
 await migrateLegacy();await hydrateHistory();refreshStatus();sendReadReceipts();return true;
}
function showPairing(){
 if(!verified){toast('Connect your ESP32 first.');return;}
 showDialog('Node pairing key',body=>{
  body.append(el('p','security-intro','The 256-bit AES key is stored in this ESP32’s NVS memory, not browser storage. Connected browsers retrieve it over the local USB cable into RAM. Pair both ESP32 nodes ONCE with the same key. Physical USB/flash access to a node can expose the key.'));
  const indicator=el('div','pair-status',secretKey?'🔒 Key stored on this ESP32':'⚠ No key stored on this ESP32');body.append(indicator);
  const field=document.createElement('textarea');field.className='pair-key-field';field.rows=2;field.placeholder='Paste the same 43-character key on the second node';field.autocomplete='off';field.spellcheck=false;body.append(field);
  const actions=el('div','pair-actions'),gen=el('button','primary-button','Generate and save'),join=el('button','quiet-button','Save pasted key'),copy=el('button','quiet-button','Copy key'),reveal=el('button','quiet-button','Show node key');
  copy.disabled=true;actions.append(gen,join,copy,reveal);body.append(actions);
  const help=el('p','','One-time setup: Generate on Board A, copy and install on Board B. Future browsers and computers connected by USB automatically load the node key. A full flash erase removes it; normal firmware flashing usually preserves NVS.');body.append(help);
  gen.onclick=async()=>{
   const raw=P.randomBytes(32);
   try{if(await installPairKey(raw)){
     field.value=P.base64url(raw);copy.disabled=false;
     indicator.textContent='🔒 Key saved on ESP32 NVS. Copy it to the other ESP32 once.';
     toast('256-bit key saved in ESP32 nonvolatile memory.');
   }}catch(e){toast('Could not save node key: '+e.message);}
  };
  join.onclick=async()=>{try{
    const raw=P.fromBase64url(field.value.trim());
    if(await installPairKey(raw)){closeDialog();toast('Pairing key saved on ESP32. Future browser sessions load it automatically.');}
   }catch(e){toast('Could not install key: '+e.message);}};
  reveal.onclick=async()=>{try{const raw=await getNodeKey();if(!raw){toast('This node has no saved key.');return;}field.value=P.base64url(raw);copy.disabled=false;toast('Key available for pairing the second board. Keep it private.');}catch(e){toast('Could not read node key: '+e.message);}};
  copy.onclick=async()=>{try{await navigator.clipboard.writeText(field.value.trim());toast('Copied. Clear your clipboard after pairing.');}catch{field.focus();field.select();toast('Select and copy manually.');}};
 });
}
async function migrateLegacy(){const oldKey='offgrid_phase13_history_'+macKey().replace(/:/g,'-');let raw=null;try{raw=localStorage.getItem(oldKey);}catch{}if(!raw)return;
 try{const old=JSON.parse(raw);if(!Array.isArray(old))return;for(const m of old.slice(-250)){
   if(typeof m.text!=='string'||!['in','out'].includes(m.direction))continue;
   const id=P.newId();const r={id,kind:'text',dir:m.direction,ts:m.ts||Date.now(),text:m.text,status: m.direction==='in'?'received':(m.status==='read'?'read':m.status==='delivered'?'delivered':'sent'),readSent:true,deliveredAckSent:true};await storeRecord(r);
  }localStorage.removeItem(oldKey);toast('Previous Phase 1.3 text history migrated into encrypted local storage.');
 }catch(e){console.warn('Legacy migration failed',e);}}
function showRename(){showDialog('Contact name',body=>{body.append(el('p','','This is a local nickname for the remote node.'));const field=document.createElement('input');field.value=activeContact;field.maxLength=36;body.append(field);const save=el('button','primary-button','Save contact name');save.onclick=()=>{setName(field.value.trim());persistName();closeDialog();};body.append(el('div','modal-actions')).append(save);setTimeout(()=>{field.focus();field.select();},40);});}
async function clearHistory(){if(!verified||!secretKey)return;if(runningTransfers||pendingRadio()||transferQueue.length){toast('Finish active transfers before clearing history.');return;}if(!confirm('Delete encrypted chat history for this ESP32 from this browser? This cannot be undone.'))return;
 await wipeStored();records=[];recordMap.clear();revokePhotos();renderHistory();toast('Local chat history cleared.');closeDialog();}
function showInfo(){showDialog('LoRa chat information',body=>{
 const rows=[['Your ESP32',localMac||'Disconnected'],['Firmware','OffGrid Messenger LoRa v1.8.0-rc.4'],
  ['Protocol','Authenticated SX1278 frames + AES-256-GCM (OGM4)'],
  ['Frequency','433 MHz (subject to applicable local RF rules)'],
  ['Profile',`${autoMode?'Automatic · ':'Manual · '}${profileLabels[currentProfile]}: SF${profileRadio.sf} / ${profileRadio.bw/1000} kHz / CR ${currentProfile==='ULTRA'?'4/8':'4/5'} / TX ${profileRadio.power} dBm`],
  ['Peer',radioTelemetry.peer],
  ['Radio RSSI',radioTelemetry.rssi===null?'Unknown':radioTelemetry.rssi+' dBm'],
  ['Security',secretKey?'AES-256-GCM key loaded from ESP32 NVS':'Not paired'],
  ['Photos / voice','Encrypted ≤48 KiB photos / ≤40 KiB Opus voice notes (up to 2 min if size permits)'],
  ['History','Browser IndexedDB encrypted using your pairing key']];
 for(const [title,value] of rows){const row=el('div','info-line');row.append(el('span','',title),el('strong','',value));body.append(row);}
 body.append(el('p','','Single gray tick = SX1278 TxDone for all fragments. Double gray = authenticated encrypted delivery receipt. Double blue = authenticated read receipt. A timeout means unconfirmed, not definitely undelivered.'));
 });}
function showSettings(){showDialog('LoRa settings',body=>{
 body.append(el('p','','OffGrid Messenger LoRa v1.8: compressed photos and short Opus voice notes are encrypted and segmented, with limited retries. Auto RF may choose Fast when both directions have safe signal margin and fall back based on losses. Manual modes stay in control. Voice recording is capped at 40 KiB and 120 seconds; natural continuous speech may reach the byte cap well before two minutes.'));
 const buttons=el('div','settings-actions');
 for(const [name,action] of [['Rename contact',showRename],['Node pairing key',showPairing],
   ['USB diagnostics',diagnoseUsb],['Automatic / manual LoRa modes',showProfileDialog],['LoRa speed test',showSpeedDiagnostics],['Clear history',()=>void clearHistory()]]){
  const button=el('button','quiet-button',name);button.onclick=action;buttons.append(button);
 }
 body.append(buttons);
 });}
$('viewerClose').onclick=closePhotoViewer;
$('viewerPrevious').onclick=()=>navigatePhoto(-1);
$('viewerNext').onclick=()=>navigatePhoto(1);
$('viewerZoom').onclick=togglePhotoZoom;
$('viewerImage').onclick=togglePhotoZoom;
$('viewerStage').addEventListener('click',e=>{if(e.target===$('viewerStage'))closePhotoViewer();});
$('viewerStage').addEventListener('touchstart',e=>{viewerTouchX=e.changedTouches[0]?.clientX??null;},{passive:true});
$('viewerStage').addEventListener('touchend',e=>{
 if(viewerTouchX===null)return;const diff=(e.changedTouches[0]?.clientX??viewerTouchX)-viewerTouchX;viewerTouchX=null;
 if(Math.abs(diff)>=55&&!viewerZoomed)navigatePhoto(diff<0?1:-1);
},{passive:true});
document.addEventListener('keydown',e=>{
 if($('photoViewer').classList.contains('hidden'))return;
 if(e.key==='Escape'){e.preventDefault();closePhotoViewer();}
 if(e.key==='ArrowLeft'){e.preventDefault();navigatePhoto(-1);}
 if(e.key==='ArrowRight'){e.preventDefault();navigatePhoto(1);}
});
$('voiceButton').onclick=()=>void toggleVoiceRecording();
document.addEventListener('keydown',e=>{if(e.key==='Escape'&&currentVoiceRecording){stopVoiceRecording(true);toast('Voice recording discarded.');}});
$('connectButton').onclick=()=>void connect();$('disconnectButton').onclick=()=>void disconnect();$('usbDiagnosticsButton').onclick=diagnoseUsb;$('sendButton').onclick=()=>void sendText();
$('messageInput').addEventListener('input',updateByteCount);$('messageInput').addEventListener('keydown',e=>{if(e.key==='Enter'&&!e.shiftKey){e.preventDefault();void sendText();}});
$('attachments').onclick=()=>$('photoPicker').click();$('photoPicker').addEventListener('change',e=>{if(e.target.files?.length)void sendPhoto(e.target.files[0]);});
$('securityButton').onclick=showPairing;$('renameButton').onclick=showRename;$('settingsButton').onclick=showSettings;$('infoButton').onclick=showInfo;
$('notificationButton').onclick=()=>void toggleDesktopNotifications();
$('themeToggle').onclick=()=>{theme=theme==='dark'?'light':'dark';document.body.classList.toggle('light',theme==='light');try{localStorage.setItem('offgrid_theme',theme);}catch{}};
$('contactButton').onclick=()=>{document.body.classList.add('mobile-chat');scrollToNewest(true);sendReadReceipts();};$('mobileBack').onclick=()=>document.body.classList.remove('mobile-chat');
$('closeModal').onclick=closeDialog;$('modalBackdrop').onclick=e=>{if(e.target===$('modalBackdrop'))closeDialog();};document.addEventListener('keydown',e=>{if(e.key==='Escape')closeDialog();});
document.addEventListener('visibilitychange',()=>{sendReadReceipts();updateNotificationButton();});window.addEventListener('focus',sendReadReceipts);
window.addEventListener('beforeunload',()=>{stopVoiceRecording(true);revokePhotos();});
setInterval(pruneIncoming,45000);
(async()=>{try{db=await openDb();}catch(e){toast('IndexedDB unavailable. Encrypted storage cannot work: '+e.message);}refreshStatus();updateUnreadBadge();updateNotificationButton();})();
console.info('OffGrid Messenger LoRa v1.8: encrypted text, compressed segmented photos and Opus voice, adaptive RF.');
