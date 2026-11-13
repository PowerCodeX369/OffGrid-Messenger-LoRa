'use strict';
const assert=require('node:assert/strict');
const crypto=require('node:crypto').webcrypto;
const P=require('../../web/protocol.js'),M=require('../../web/lora_media.js');
async function test(kind,size,mime){
 const key=await P.importKey(crypto.getRandomValues(new Uint8Array(32)));
 const original=crypto.getRandomValues(new Uint8Array(size));
 const pieces=await M.split(kind,mime,original);
 assert(pieces.length>=1 && pieces.length<=10);
 const receive=new M.Reassembler(3),id=P.newId(),sender='AB:CD:EF:12:34:56';
 let result=null;
 const schedule=[...pieces.keys()].reverse();
 // An out-of-order retransmit and duplicate must not corrupt the media.
 schedule.splice(1,0,schedule[0]);
 for(const index of schedule){
   let tid=P.newId();while(tid.slice(0,8)==='00000000'||tid.slice(8)==='00000000')tid=P.newId();
   const plain=P.makeEnvelope(P.types.mediaPart,id,pieces[index],1700000000,kind==='image'?900:5000,kind==='image'?600:0);
   const encrypted=await P.seal(key,plain,tid);
   assert(encrypted.length<=7168,encrypted.length);
   const opened=await P.open(key,encrypted,tid),env=P.parseEnvelope(opened);
   assert.equal(env.kind,P.types.mediaPart);
   const res=await receive.ingest(sender,id,env.body,{width:env.width,height:env.height,ts:env.ts});
   if(res.complete)result=res;
 }
 assert(result?.complete);
 assert.equal(result.kind,kind);
 assert.equal(result.mime,mime);
 assert.deepEqual(result.bytes,original);
 assert.equal(receive.inflight.size,0);
 // Substituting one authenticated segment of another image is not accepted.
 const r=new M.Reassembler();let tamper=pieces[0].slice();tamper[25]^=1;
 await r.ingest(sender,id,tamper,{width:1,height:1,ts:1});
 if(pieces.length>1) await assert.rejects(()=>r.ingest(sender,id,pieces[1],{width:1,height:1,ts:1}),/Conflicting/);
 // Cross-transfer AES-GCM packet manipulation must fail.
 const tid=P.newId();const c=await P.seal(key,P.makeEnvelope(P.types.mediaPart,id,pieces[0]),tid);
 c[c.length-1]^=1;
 await assert.rejects(()=>P.open(key,c,tid));
 console.log(`PASS: ${kind} ${size} bytes, ${pieces.length} segments, out-of-order, duplicate, SHA-256, GCM`);
}
(async()=>{
 await test('image',48*1024,'image/webp');
 await test('voice',40*1024,'audio/webm');
 await assert.rejects(()=>M.split('image','image/jpeg',new Uint8Array(51*1024)),/budget/);
 await assert.rejects(()=>M.split('voice','audio/webm',new Uint8Array(41*1024)),/budget/);
 assert.equal(M.parse(new Uint8Array(32)),null);
 const r=new M.Reassembler();const part=(await M.split('voice','audio/webm',new Uint8Array(7000)))[0];
 await r.ingest('A','0123456789ABCDEF',part,{width:500,height:0,ts:1});
 await assert.rejects(()=>r.ingest('A','0123456789ABCDEF',part,{width:501,height:0,ts:1}),/Conflicting/);
 console.log('PASS: strict media size, metadata, and codec validation');
})().catch(e=>{console.error(e);process.exitCode=1;});
