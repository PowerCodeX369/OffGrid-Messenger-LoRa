/* Android v2.0: preserve the unmodified LoRa browser application and protocol.
 * This is a Web Serial facade over an origin-constrained Android native transport.
 * No secret keys are stored or logged here; encrypted payloads remain OGM4. */
(function(root) {
'use strict';
const pending = new Map();
let sequence = 0;
const utf8 = new TextEncoder();
const decoder = new TextDecoder();
function send(type, data = {}) {
  if (!root.OffGridNative || typeof root.OffGridNative.postMessage !== 'function')
    return Promise.reject(Error('Android native transport is unavailable'));
  const id = ++sequence;
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => { pending.delete(id); reject(Error('Android transport timeout')); }, type === 'open' ? 90000 : 30000);
    pending.set(id, {resolve, reject, timer});
    try { root.OffGridNative.postMessage(JSON.stringify(Object.assign({type, id}, data))); }
    catch (e) {clearTimeout(timer);pending.delete(id);reject(e);}
  });
}
function toBase64(bytes) {
  let result = '';
  for (let i = 0; i < bytes.length; i += 2048)
    result += String.fromCharCode(...bytes.subarray(i, i + 2048));
  return btoa(result);
}
function fromBase64(encoded) {
  const value = atob(encoded);
  const result = new Uint8Array(value.length);
  for(let i = 0; i < value.length; i++) result[i] = value.charCodeAt(i);
  return result;
}
let active = null;
root.__offgridAndroidSerial = Object.freeze({
  receive(message) {
    if(!message || typeof message !== 'object') return;
    if(message.type === 'result') {
      const item = pending.get(message.id);
      if(!item) return;
      clearTimeout(item.timer);pending.delete(message.id);
      if(message.ok) item.resolve();
      else item.reject(Error(String(message.error || 'Android transport failed')));
    } else if(message.type === 'rx') {
      try {if(active) active.deliver(fromBase64(message.data));}catch(e){console.warn('USB/BLE receive failed:',e.message);}
    } else if(message.type === 'disconnect') {
      active?.lost(String(message.reason || 'Transport disconnected'));
    }
  }
});
class AndroidSerialPort {
  constructor() { this.readable = null; this.writable = null; this.controller = null; this.opened = false; this.incoming = []; }
  async open(options = {}) {
    if(this.opened) throw Error('Port already open');
    active = this;
    try { await send('open', {baudRate:options.baudRate || 115200}); }
    catch (e) { if(active===this)active=null; throw e; }
    this.opened = true;
    this.readable = new ReadableStream({
      start: (controller) => { this.controller = controller; for (const item of this.incoming.splice(0)) controller.enqueue(item); },
      cancel: () => { this.controller = null; }
    });
    this.writable = new WritableStream({
      write: (chunk) => {
        if(!this.opened) return Promise.reject(Error('Port closed'));
        const bytes = chunk instanceof Uint8Array ? chunk : new Uint8Array(chunk);
        if(bytes.length > 16384) return Promise.reject(Error('Native write too large'));
        return send('write', {data:toBase64(bytes)});
      }
    });
  }
  deliver(bytes) {
    if(!this.opened && active!==this) return;
    if(this.controller) this.controller.enqueue(bytes);
    else if(this.incoming.length < 48) this.incoming.push(bytes);
  }
  lost(reason) {
    this.opened = false;
    try {this.controller?.error(Error(reason));} catch {}
    this.controller = null;
    if(active === this) active = null;
  }
  async close() {
    if(!this.opened)return;
    this.opened=false;
    if(active===this)active=null;
    try {await send('close');}finally {try{this.controller?.close();}catch{}this.controller=null;}
  }
}
/* Native device features unavailable in many Android WebView builds. */
let notificationPermission = 'default';
class AndroidNotification {
  constructor(title, options={}) {
    this.onclick = null;
    this.title = String(title || 'OffGrid Messenger');
    send('notify', {title:this.title, body:String(options.body || '').slice(0,250), tag:String(options.tag || '').slice(0,80)}).catch(()=>{});
  }
  close() {}
  static get permission() { return notificationPermission; }
  static async requestPermission() {
    try {await send('notifyPermission');notificationPermission='granted';}
    catch {notificationPermission='denied';}
    return notificationPermission;
  }
}
root.Notification = AndroidNotification;
if(!navigator.clipboard || typeof navigator.clipboard.writeText !== 'function') {
  Object.defineProperty(navigator,'clipboard',{configurable:true,value:{writeText:(value)=>send('clipboard',{text:String(value).slice(0,24000)})}});
}
/* Save the existing photo viewer's Blob download using Android's system
 * document picker, rather than relying on unsupported WebView blob downloads. */
if(typeof document !== 'undefined') document.addEventListener('click',async(event)=>{
  const anchor=event.target?.closest?.('a[download]');
  if(!anchor || !anchor.href.startsWith('blob:'))return;
  event.preventDefault();
  try {
    const response=await fetch(anchor.href);const blob=await response.blob();
    if(blob.size>100*1024)throw Error('File exceeds safe Android export limit');
    const bytes=new Uint8Array(await blob.arrayBuffer());
    await send('savePhoto',{name:String(anchor.download||'offgrid-photo.webp').slice(0,80),data:toBase64(bytes)});
  }catch(e){console.warn('Android photo save:',e.message);}
},true);
const serial = Object.freeze({
  async requestPort() { return new AndroidSerialPort(); },
  async getPorts() { return []; },
  addEventListener() {},removeEventListener() {}
});
Object.defineProperty(navigator,'serial',{configurable:true,get(){return serial;}});
root.OffGridAndroidApp = true;
// No override of crypto, IndexedDB, MediaRecorder, photo compression or RF controls.
})(typeof globalThis === 'object' ? globalThis : window);
