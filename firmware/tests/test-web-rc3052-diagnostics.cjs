'use strict';
const assert=require('node:assert/strict');
const fs=require('node:fs');
const {BulkLink}=require('../../web/lora_bulk.js');
const source=fs.readFileSync('web/app.js','utf8');
const html=fs.readFileSync('web/index.html','utf8');
assert(source.includes('function compactLinkDiagnostic(line)'));
assert(source.includes("'PEER_ARQ'"));
assert(source.includes("'ARQ_ACKRX'"));
assert(source.includes("'ARQ_BADMAC'"));
assert(source.includes('keyFingerprint=await radioKeyFingerprint(raw)'));
assert(source.includes("if(prior==='delivered'&&!['delivered','read'].includes(status))return;"));
assert(!source.includes('const urgentIndex=transferQueue.findIndex'));
assert(source.includes('const job=transferQueue.shift();'));
assert(html.includes('rc3052diag'));
(async()=>{
 const id='0011223344556677',payload=new Uint8Array(93).fill(0x6a),seen=[];
 let link;
 link=new BulkLink(async command=>{
  assert(command.includes('BULKBEGIN '+id));
  assert(command.includes('BULKCHUNK '+id));
  assert(command.includes('BULKEND '+id));
  setImmediate(()=>{
   link.onLine('BULKQUEUED|'+id);
   setImmediate(()=>{
    link.onLine('BULKSENT|'+id);
    link.onLine('BULKPROGRESS|'+id+'|1|1');
    link.onLine('BULKCONFIRMED|'+id);
   });
  });
 },()=>{});
 const m=await link.send(id,payload,false,{onStage:s=>seen.push(s)});
 assert(m.transferMs>=0);
 assert(seen.includes('BULKQUEUED_SUBMITTED'));
 assert(seen.includes('BULKQUEUED_ACKED'));
 assert(seen.includes('BULKSENT'));
 assert(seen.includes('BULKCONFIRMED'));
 let rejectLink;
 rejectLink=new BulkLink(async()=>{
  setImmediate(()=>rejectLink.onLine('BULKFAILED|'+id+'|BAD_CRC'));
 },()=>{});
 const rejected=[];
 await assert.rejects(rejectLink.send(id,payload,false,{onStage:x=>rejected.push(x)}),/BAD_CRC/);
 assert(rejected.includes('BULKFAILED:BAD_CRC'));
 console.log('PASS: rc2 diagnostic stages, actual bulk confirmation/failure, key comparison, monotonic receipts, v3.0.4 FIFO');
})().catch(e=>{console.error(e);process.exitCode=1;});
