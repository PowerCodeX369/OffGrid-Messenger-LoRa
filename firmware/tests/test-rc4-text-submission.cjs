'use strict';
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const script=fs.readFileSync('web/app.js','utf8');
const begin=script.indexOf('let sendTextBusy=false;');
const end=script.indexOf('function updatePrimaryAction()',begin);
assert(begin>0&&end>begin,'Native browser send handler must exist');
const snippet=script.slice(begin,end);
assert(!snippet.includes('input.value=\'\';updateByteCount();\n if(transferQueue'),'Composer must clear before first IndexedDB await');
let unblock,number=0;
const input={value:'hi'},records=[],jobs=[],toasts=[];
const gate=new Promise(r=>{unblock=r;});
const scope={
  profileTransition:false,verified:true,secretKey:Uint8Array.of(1),
  $:(name)=>{assert.equal(name,'messageInput');return input;},
  P:{newId:()=>String(++number).padStart(16,'0')},
  countChars:(t)=>[...t].length,MAX_CHARS:500,MAX_TEXT_QUEUE:30,
  performance:{now:()=>100},Date,toast:(s)=>toasts.push(s),
  updateByteCount:()=>{},
  appendRecord:async(r)=>{records.push(r);if(records.length===1)await gate;},
  weakTextRetry:{attempt:()=>{}},transferQueue:jobs,
  setState:async()=>{},makeJob:(r)=>({id:r.id,body:r.text}),runTransfers:()=>{}
};
vm.createContext(scope);vm.runInContext(snippet,scope);
(async()=>{
 const first=scope.sendText();
 const second=scope.sendText();
 assert.equal(records.length,1,'rapid double-click must not create duplicate message IDs');
 assert.equal(input.value,'','composer must clear synchronously before storage');
 unblock();await Promise.all([first,second]);
 assert.equal(jobs.length,1,'only one queued RF job after async save');
 assert.equal(records[0].text,'hi');
 input.value='hi';await scope.sendText();
 assert.equal(jobs.length,2,'intentional new message after first completes remains possible');
 assert.notEqual(jobs[0].id,jobs[1].id);
 console.log('PASS: asynchronous IndexedDB save cannot duplicate one chat Send; intentional subsequent send preserved');
})().catch(err=>{console.error(err);process.exitCode=1;});
