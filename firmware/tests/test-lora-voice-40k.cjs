'use strict';
const assert=require('node:assert/strict');
const P=require('../../web/protocol.js');
const M=require('../../web/lora_media.js');
(async()=>{
 assert.equal(P.VOICE_MAX,40*1024);
 assert.equal(P.VOICE_MAX_MS,120000);
 assert.equal(M.MAX_VOICE,40*1024);
 // Short audio stays short: no fake 40 KiB padding on encryption framing.
 for(const [size,duration] of [[600,1000],[1400,2000],[20*1024,60000],[40*1024,120000]]){
   const b=new Uint8Array(size);b[0]=1;b[b.length-1]=2;
   const packed=P.packVoice('audio/webm',b);
   assert.equal(packed.byteLength,size+1);
   const restored=P.unpackVoice(packed,duration);
   assert.equal(restored.bytes.byteLength,size);
   assert.deepEqual(restored.bytes,b);
   const parts=await M.split('voice','audio/webm',b);
   assert(parts.length<=10);
   const rec=new M.Reassembler();let result;
   for(let i=parts.length-1;i>=0;i--) result=await rec.ingest('aa:bb:cc','ABCDEF0123456789',parts[i],{ts:1});
   assert.equal(result.complete,true);
   assert.equal(result.bytes.byteLength,size);
 }
 assert.throws(()=>P.packVoice('audio/webm',new Uint8Array(40*1024+1)),/too large/);
 await assert.rejects(()=>M.split('voice','audio/webm',new Uint8Array(40*1024+1)),/budget/);
 assert.throws(()=>P.unpackVoice(new Uint8Array(300),120001),/Invalid voice/);
 assert.equal(M.MAX_PHOTO,50*1024); // photo limit unchanged
 await assert.rejects(()=>M.split('image','image/jpeg',new Uint8Array(50*1024+1)),/budget/);
 console.log('PASS: voice 40 KiB cap, no short-clip padding, 120-second metadata, photo budget unchanged');
})().catch(e=>{console.error(e);process.exitCode=1;});
