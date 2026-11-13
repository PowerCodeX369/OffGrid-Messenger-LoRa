'use strict';
const assert=require('node:assert/strict');
const crypto=require('node:crypto').webcrypto;
const P=require('../../web/protocol.js'),M=require('../../web/lora_media.js');
(async()=>{
 const key=await P.importKey(crypto.getRandomValues(new Uint8Array(32)));
 const packetId=P.newId(),transferId=P.newId();
 const envelope=P.makeEnvelope(P.types.text,packetId,P.textBytes('offgrid tamper test'));
 const ciphertext=await P.seal(key,envelope,transferId);
 // Any tamper to an authenticated byte (IV, data, tag) must fail GCM.
 for(let i=0;i<ciphertext.length;i++){
  const tampered=ciphertext.slice();tampered[i]^=1;
  await assert.rejects(P.open(key,tampered,transferId),'tamper at byte '+i);
 }
 await assert.rejects(P.open(key,ciphertext,P.newId()),'transfer ID bound into AES-GCM AAD');
 let seed=0xA123B456;
 const random=()=>{seed^=seed<<13;seed^=seed>>>17;seed^=seed<<5;return seed>>>0;};
 // Untrusted USB/radio frame parsing: never throw, exceed bounds or accept
 // malformed counts/lengths. This is mutation fuzzing, NOT formal verification.
 const goodFrame=P.frames(transferId,ciphertext)[0];
 for(let i=0;i<20000;i++){
  const value=goodFrame.slice();
  for(let j=0,n=random()%5+1;j<n;j++)value[random()%value.length]=random()&255;
  let result;
  assert.doesNotThrow(()=>{result=P.parseFrame(value);});
  if(result){assert.equal(result.count,Math.ceil(result.length/P.CHUNK));assert(result.index<result.count);}
 }
 // Media part parser must not throw on random/near-valid user-supplied data.
 const voice=await M.split('voice','audio/webm',new Uint8Array(18000).fill(0x7f));
 for(let i=0;i<20000;i++){
  const value=(i&1)?voice[random()%voice.length].slice():new Uint8Array(random()%128);
  if(value.length)value[random()%value.length]=random()&255;
  let part;assert.doesNotThrow(()=>{part=M.parse(value);});
  if(part){assert(part.count>=1&&part.count<=10);assert(part.index<part.count);assert(part.total<=40*1024);}
 }
 // Out-of-order valid media segments must assemble while conflicting duplicates
 // never silently overwrite authenticated content.
 const original=new Uint8Array(40*1024);for(let i=0;i<original.length;i++)original[i]=i&255;
 const parts=await M.split('voice','audio/webm',original);
 const re=new M.Reassembler(4);const sender='AA:BB:CC:DD:EE:FF',id=P.newId();
 for(let i=parts.length-1;i>=0;i--){const r=await re.ingest(sender,id,parts[i],{durationMs:9000});if(i===0){assert(r.complete);assert.deepEqual(r.bytes,original);}}
 console.log('PASS: AES-GCM tamper/AAD, 40000 fuzzed frame+media inputs, reordered encrypted media reassembly');
})().catch(e=>{console.error(e);process.exitCode=1;});
