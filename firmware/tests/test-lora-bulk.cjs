'use strict';
const assert=require('node:assert/strict');
const {BulkLink,crc32,hex32,MAX}=require('../../web/lora_bulk.js');
const P=require('../../web/protocol.js');
const id='1234567890ABCDEF';
(async()=>{
 assert.equal(hex32(crc32(new TextEncoder().encode('123456789'))),'CBF43926');
 let link,once=0,sent=0,progress=0;
 let staged=[],expectedLen=0,steps=0,crc='';
 const write=async line=>{
  const fields=line.split(' ');
  if(fields[0]==='BULKBEGIN'){
   expectedLen=Number(fields[2]);
   setImmediate(()=>link.onLine(`BULKREADY|${id}`));
  }else if(fields[0]==='BULKCHUNK'){
   assert.equal(Number(fields[2]),steps);
   staged.push(P.unhex(fields[3]));
   const seq=steps++;
   setImmediate(()=>link.onLine(`BULKSTEP|${id}|${seq}`));
  }else if(fields[0]==='BULKEND'){
   crc=fields[2];
   setImmediate(()=>{
    link.onLine(`BULKQUEUED|${id}`);
    link.onLine(`BULKSENT|${id}`);
    link.onLine(`BULKPROGRESS|${id}|5|5`);
    link.onLine(`BULKCONFIRMED|${id}`);
   });
  }else throw Error('Unexpected serial command: '+line);
 };
 let rx=[];
 link=new BulkLink(write,async (mac,receivedId,cipher)=>rx.push({mac,id:receivedId,cipher}));
 const payload=Uint8Array.from({length:1024},(_,i)=>(i*17+7)&255);
 await link.send(id,payload,false,{onSent:()=>sent++,onProgress:(n,total)=>{progress=n/total;}});
 const actual=P.concat(...staged);
 assert.equal(expectedLen,1024);assert.equal(steps,7);
 assert.deepEqual(actual,payload);assert.equal(crc,hex32(crc32(payload)));
 assert.equal(sent,1);assert.equal(progress,1);assert.equal(link.stats.confirmed,1);
 const peer='AA:BB:CC:DD:EE:FF';
 const rxid='AA11223344AABBCC';
 assert(link.onLine(`RXBSTART|${rxid}|${payload.length}|${peer}`));
 for(let at=0,seq=0;at<payload.length;at+=160,seq++){
  assert(link.onLine(`RXBCHUNK|${rxid}|${seq}|${P.hex(payload.subarray(at,at+160))}`));
 }
 assert(link.onLine(`RXBDONE|${rxid}|${hex32(crc32(payload))}`));
 await new Promise(r=>setImmediate(r));
 assert.equal(rx.length,1);assert.equal(rx[0].id,rxid);assert.equal(rx[0].mac,peer);
 assert.deepEqual(rx[0].cipher,payload);
 // Broken CRC is dropped, never delivered to AES-GCM.
 link.onLine(`RXBSTART|${rxid}|32|${peer}`);
 link.onLine(`RXBCHUNK|${rxid}|0|${P.hex(payload.subarray(0,32))}`);
 link.onLine(`RXBDONE|${rxid}|00000000`);
 await new Promise(r=>setImmediate(r));
 assert.equal(rx.length,1);
 assert.throws(()=>crc32(null));
 await assert.rejects(()=>link.send('0000000090ABCDEF',payload,false));
 await assert.rejects(()=>link.send(id,new Uint8Array(MAX+1),false));
 link.cancel();await assert.rejects(()=>link.send(id,payload,false));
 console.log('PASS: browser bulk 1024-byte staging, positive backpressure, sent+bitmap confirmation, CRC RX bounds');
})().catch(e=>{console.error(e);process.exitCode=1;});
