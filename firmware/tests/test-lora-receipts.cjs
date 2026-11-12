'use strict';
const assert=require('node:assert/strict');
const R=require('../../web/lora_receipts.js');
const P=require('../../web/protocol.js');
const ids=Array.from({length:8},(_,i)=>({kind:i%2?'read':'delivered',targetId:i.toString(16).padStart(16,'0').toUpperCase()}));
const packed=R.pack(ids);
assert.equal(packed.length,73);
assert.deepEqual(R.unpack(packed),ids);
const env=P.makeEnvelope(P.types.receiptBatch,P.newId(),packed);
assert.equal(env.length,97); // +28 bytes AES-GCM => 125, one 160-byte fragment
assert.equal(P.frames(P.newId(),new Uint8Array(env.length+28)).length,1);
assert.throws(()=>R.pack([...ids,ids[0]]));
assert.throws(()=>R.pack([ids[0],ids[0]]));
const corrupt=packed.slice();corrupt[1]=99;assert.throws(()=>R.unpack(corrupt));
assert.throws(()=>R.unpack(packed.subarray(0,packed.length-1)));
(async()=>{
 const key=await P.importKey(P.randomBytes(32));const transfer=P.newId();
 const sealed=await P.seal(key,env,transfer);
 const opened=P.parseEnvelope(await P.open(key,sealed,transfer));
 assert.deepEqual(R.unpack(opened.body),ids);
 sealed[0]^=1;
 await assert.rejects(()=>P.open(key,sealed,transfer));
 for(const n of [64,256,512,1024]){
  const probeId=P.newId();const probe=P.makeEnvelope(P.types.probe,probeId,P.randomBytes(n));
  const txid=P.newId();const encrypted=await P.seal(key,probe,txid);
  const frames=P.frames(txid,encrypted);assert(frames.length>=1);
  const combined=P.concat(...frames.map(f=>P.parseFrame(f).bytes));
  const decoded=P.parseEnvelope(await P.open(key,combined,txid));
  assert.equal(decoded.kind,P.types.probe);assert.equal(decoded.body.length,n);
  const answer=P.makeEnvelope(P.types.probeAck,P.newId(),P.unhex(probeId));
  assert.equal(P.parseEnvelope(answer).body.length,8);
 }
 console.log('PASS: eight authenticated delivery/read receipts share one LoRa fragment; tampering rejected');
})().catch(e=>{console.error(e);process.exitCode=1;});
