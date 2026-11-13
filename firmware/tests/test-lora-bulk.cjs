'use strict';
const assert=require('node:assert/strict');
const {BulkLink,crc32,hex32,MAX}=require('../../web/lora_bulk.js');
const P=require('../../web/protocol.js');
const id='1234567890ABCDEF';
(async()=>{
 assert.equal(hex32(crc32(new TextEncoder().encode('123456789'))),'CBF43926');
 let link,once=0,sent=0,progress=0;
 let staged=[],expectedLen=0,steps=0,crc='',writes=0,stageCheckpoints=0;
 const write=async batch=>{
  writes++;
  for(const line of batch.split('\n')){
  const fields=line.split(' ');
  if(fields[0]==='BULKBEGIN'){
   expectedLen=Number(fields[2]);
   setImmediate(()=>link.onLine(`BULKREADY|${id}`));
  }else if(fields[0]==='BULKCHUNK'){
   assert.equal(Number(fields[2]),steps);
   staged.push(P.unhex(fields[3]));
   const seq=steps++;
   if((seq+1)%4===0||staged.reduce((n,b)=>n+b.length,0)===expectedLen){
    stageCheckpoints++;
    setImmediate(()=>link.onLine(`BULKSTEP|${id}|${seq}`));
   }
  }else if(fields[0]==='BULKEND'){
   crc=fields[2];
   setImmediate(()=>{
    link.onLine(`BULKQUEUED|${id}`);
    link.onLine(`BULKSENT|${id}`);
    link.onLine(`BULKPROGRESS|${id}|5|5`);
    link.onLine(`BULKSTAT|${id}|RF_MS=615|FRAMES=5|RETX=0|TIMEOUT=0`);
    link.onLine(`BULKCONFIRMED|${id}`);
   });
  }else throw Error('Unexpected serial command: '+line);
  }
 };
 let rx=[];
 link=new BulkLink(write,async (mac,receivedId,cipher)=>rx.push({mac,id:receivedId,cipher}));
 const payload=Uint8Array.from({length:1024},(_,i)=>(i*17+7)&255);
 const metrics=await link.send(id,payload,false,{onSent:()=>sent++,onProgress:(n,total)=>{progress=n/total;}});
 assert.equal(metrics.RF_MS,615);assert.equal(metrics.FRAMES,5);assert.equal(metrics.RETX,0);
 assert(metrics.usbStageMs>=0 && metrics.radioWaitMs>=0);
 const actual=P.concat(...staged);
 assert.equal(expectedLen,1024);assert.equal(steps,7);
 assert.equal(writes,3);assert.equal(stageCheckpoints,2);
 assert.deepEqual(actual,payload);assert.equal(crc,hex32(crc32(payload)));
 assert.equal(sent,1);assert.equal(progress,1);assert.equal(link.stats.confirmed,1);
 assert.equal(link.stats.failed,0);
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
 // A one-chunk encrypted text or receipt must take exactly ONE USB write.
 // Firmware validates all three line commands in order and sends BULKQUEUED
 // only after sequence and whole-transfer CRC pass.
 const shortId='12345678ABCDEF90';
 const short=Uint8Array.from({length:96},(_,i)=>i*7&255);
 let shortLink,shortWrites=0,shortStages=[],shortFailed=false;
 shortLink=new BulkLink(async batch=>{
  shortWrites++;
  shortStages=batch.split('\n');
  assert.equal(shortStages.length,3,'short transfer uses one three-line write');
  assert.match(shortStages[0],/^BULKBEGIN /);
  assert.match(shortStages[1],/^BULKCHUNK /);
  assert.match(shortStages[2],/^BULKEND /);
  assert.deepEqual(P.unhex(shortStages[1].split(' ')[3]),short);
  assert.equal(shortStages[2].split(' ')[2],hex32(crc32(short)));
  setImmediate(()=>{
   shortLink.onLine(`BULKREADY|${shortId}`);
   shortLink.onLine(`BULKSTEP|${shortId}|0`);
   shortLink.onLine(`BULKQUEUED|${shortId}`);
   shortLink.onLine(`BULKSTAT|${shortId}|RF_MS=180|FRAMES=1|RETX=0|TIMEOUT=0`);
   shortLink.onLine(`BULKCONFIRMED|${shortId}`);
  });
 },()=>{});
 const shortMetrics=await shortLink.send(shortId,short,true);
 assert.equal(shortWrites,1);
 assert.equal(shortMetrics.FRAMES,1);
 assert.equal(shortMetrics.RF_MS,180);
 // Any firmware-rejected stage must reject, even if all commands were sent.
 const rejectId='12345678ABCDEF91';
 let badLink;
 badLink=new BulkLink(async batch=>{
  assert.equal(batch.split('\n').length,3);
  setImmediate(()=>badLink.onLine(`BULKFAILED|${rejectId}|STAGING_CRC_OR_LENGTH`));
 },()=>{});
 await assert.rejects(()=>badLink.send(rejectId,short,false),/STAGING_CRC_OR_LENGTH/);
 assert.equal(badLink.stats.failed,1);

 // Exercise a near-maximum encrypted media part over the unchanged firmware's
 // every-four-chunk checkpoints. Every browser write must fit the 4-KiB RX buffer.
 const id2='1122334455667788';let other,off2=0,seq2=0,maxBatchBytes=0,writes2=0;
 const large=Uint8Array.from({length:7120},(_,i)=>(i*31)&255);
 const secondWrite=async batch=>{
  writes2++;maxBatchBytes=Math.max(maxBatchBytes,new TextEncoder().encode(batch).length+1);
  for(const line of batch.split('\n')){
   const p=line.split(' ');
   if(p[0]==='BULKBEGIN'){
    assert.equal(Number(p[2]),large.length);
    setImmediate(()=>other.onLine(`BULKREADY|${id2}`));
   }else if(p[0]==='BULKCHUNK'){
    assert.equal(Number(p[2]),seq2);
    const bytes=P.unhex(p[3]);
    assert.deepEqual(bytes,large.subarray(off2,off2+bytes.length));
    off2+=bytes.length;const seq=seq2++;
    if((seq+1)%4===0||off2===large.length)
      setImmediate(()=>other.onLine(`BULKSTEP|${id2}|${seq}`));
   }else if(p[0]==='BULKEND'){
    assert.equal(p[2],hex32(crc32(large)));
    setImmediate(()=>{other.onLine(`BULKQUEUED|${id2}`);
      other.onLine(`BULKSTAT|${id2}|RF_MS=2500|FRAMES=32|RETX=0|TIMEOUT=0`);
      other.onLine(`BULKCONFIRMED|${id2}`);});
   }else throw Error('Unexpected bulk command');
  }
 };
 other=new BulkLink(secondWrite,()=>{});
 const bigMetrics=await other.send(id2,large,false);
 assert.equal(off2,large.length);
 assert.equal(seq2,Math.ceil(large.length/160));
 assert.equal(writes2,8); // BEGIN + 6 × eight-line-or-less stage writes + END.
 assert(maxBatchBytes<4096,maxBatchBytes);
 assert.equal(bigMetrics.FRAMES,32);
 console.log('PASS: short text/receipt takes one CRC-checked USB write; rejection propagates; 1024/7120-byte batches preserve sequence/CRC and bitmap confirmation');
})().catch(e=>{console.error(e);process.exitCode=1;});
