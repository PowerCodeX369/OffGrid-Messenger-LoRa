'use strict';
const assert=require('node:assert/strict');
const {FragmentScheduler}=require('../../web/lora_scheduler.js');
const {summarize}=require('../../web/lora_benchmark.js');
const pause=ms=>new Promise(resolve=>setTimeout(resolve,ms));
(async()=>{
 const gate=new FragmentScheduler(12),order=[];
 let release;
 const blocker=new Promise(resolve=>release=resolve);
 const first=gate.submit('data',async()=>{order.push('data1');await blocker;return 1;});
 const second=gate.submit('data',async()=>{order.push('data2');return 2;});
 const ack=gate.submit('receipt',async()=>{order.push('receipt');return 3;});
 await pause(0);release();
 assert.deepEqual(await Promise.all([first,second,ack]),[1,2,3]);
 assert.deepEqual(order,['data1','receipt','data2']);
 assert.equal(gate.depth,0);
 // A 50-message burst must never issue simultaneous radio writes.
 const flood=new FragmentScheduler(60);let concurrency=0,maxConcurrency=0;
 const floodJobs=[];
 for(let i=0;i<50;i++)floodJobs.push(flood.submit(i%5===0?'receipt':'data',async()=>{
   concurrency++;maxConcurrency=Math.max(maxConcurrency,concurrency);
   await pause(0);concurrency--;return i;
 }));
 assert.equal((await Promise.all(floodJobs)).length,50);
 assert.equal(maxConcurrency,1);
 // A receiver continuously producing read receipts must not starve text.
 const fair=new FragmentScheduler(16),fairOrder=[];
 let release2;
 const b=new Promise(resolve=>release2=resolve);
 const b0=fair.submit('receipt',async()=>{fairOrder.push('r0');await b;});
 const jobs=[];
 for(let i=1;i<=7;i++)jobs.push(fair.submit('receipt',async()=>{fairOrder.push('r'+i);}));
 jobs.push(fair.submit('data',async()=>{fairOrder.push('data');}));
 release2();await Promise.all([b0,...jobs]);
 assert(fairOrder.indexOf('data')>=1&&fairOrder.indexOf('data')<=5,fairOrder);
 // A disconnected session cannot leak queued frames into a new one.
 const pending=new FragmentScheduler(2);let stop;
 const hold=new Promise(resolve=>stop=resolve);
 const started=pending.submit('data',()=>hold);
 const queued=pending.submit('data',()=>4);
 const rejected=queued.then(()=>false,e=>/closed/.test(e.message));
 assert.equal(await pending.submit('data',()=>5).then(()=>false,e=>/full/.test(e.message)),true);
 pending.close();stop(1);await started;
 assert.equal(await rejected,true);
 assert.equal(await pending.submit('receipt',()=>5).then(()=>false,e=>/closed/.test(e.message)),true);
 // Confirmed payload throughput includes time spent on failed trials.
 const r=summarize([{ok:true,ms:120,bytes:256},{ok:false,ms:9000,bytes:0},{ok:true,ms:280,bytes:256}],10000);
 assert.equal(r.ok,2);assert.equal(r.lost,1);assert.equal(r.goodputBps,51.2);
 assert.equal(r.medianMs,120);assert.equal(r.p95Ms,280);
 console.log('PASS: serialized RF, receipt precedence, fairness, queue bounds, reconnect cancellation, measured goodput');
})().catch(error=>{console.error(error);process.exitCode=1;});
