'use strict';
const fs=require('fs'),vm=require('vm'),assert=require('assert').strict;
const path=require('path');
const source=fs.readFileSync(path.join(__dirname,'../app/src/main/assets/www/android_serial.js'),'utf8');
const writes=[], events={};
let context;
const native={postMessage(raw){
 const msg=JSON.parse(raw);writes.push(msg);
 queueMicrotask(()=>{
  if(msg.type==='open') {
   context.__offgridAndroidSerial.receive({type:'rx',data:Buffer.from('READY|node\n').toString('base64')});
  }
  context.__offgridAndroidSerial.receive({type:'result',id:msg.id,ok:true});
 });
}};
const document={addEventListener(name,callback){events[name]=callback}};
const sandbox={
  OffGridNative:native,document,navigator:{},TextEncoder,TextDecoder,
  ReadableStream,WritableStream,Uint8Array,Promise,console,setTimeout,clearTimeout,fetch:async()=>({blob:async()=>new Blob([Buffer.from('photo')])}),Blob,
  btoa(s){return Buffer.from(s,'binary').toString('base64')},
  atob(s){return Buffer.from(s,'base64').toString('binary')}
};
sandbox.globalThis=sandbox;context=vm.createContext(sandbox);vm.runInContext(source,context,{filename:'android_serial.js'});
(async()=>{
 assert(context.OffGridAndroidApp);
 const port=await context.navigator.serial.requestPort();
 await port.open({baudRate:115200});
 const reader=port.readable.getReader();
 const result=await reader.read();assert.equal(Buffer.from(result.value).toString(),'READY|node\n');
 const writer=port.writable.getWriter();
 await writer.write(new Uint8Array(Buffer.from('BULKBEGIN ABC 1024 DATA\n')));
 const sent=writes.find(m=>m.type==='write');assert(sent);
 assert.equal(Buffer.from(sent.data,'base64').toString(),'BULKBEGIN ABC 1024 DATA\n');
 await context.Notification.requestPermission();assert.equal(context.Notification.permission,'granted');
 new context.Notification('OffGrid',{body:'New encrypted message'});
 await events.click({target:{closest(){return {href:'blob:photo',download:'photo.webp'}}},preventDefault(){}});
 assert(writes.find(m=>m.type==='savePhoto'));
 await writer.close();reader.releaseLock();await port.close();
 assert(writes.some(m=>m.type==='close'));
 assert(!source.includes('KEYDATA|'));
 console.log('PASS: Android Web Serial shim native open, reads, ordered writes, notifications, blob export and close');
})().catch(e=>{console.error(e);process.exitCode=1});
