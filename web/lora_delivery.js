/* LoRa v1.1: small testable delivery policy. No UI or network dependencies. */
(function(root){'use strict';
 const receiptTimeoutMs = (fragments,ultra=false) => {
  if(!Number.isInteger(fragments)||fragments<1)throw Error('invalid fragment count');
  return ultra?Math.min(480000,Math.max(180000,90000+fragments*30000)):
   Math.min(90000,Math.max(15000,9000+fragments*2400));
 };
 /* True only once every fragment has actually completed SX1278 TX.
  * A radio ACK, by itself, must never produce a delivered/read tick. */
 const markTxDone=(job,index)=>{
  if(!Number.isInteger(index)||index<0||!Number.isInteger(job.totalFrames)||
     index>=job.totalFrames)throw Error('invalid transmitted frame');
  if(!job.transmitted)job.transmitted=new Set();
  job.transmitted.add(index);
  return job.transmitted.size===job.totalFrames;
 };
 const encryptedReceiptStatus=kind=>{
  if(kind==='delivered')return 'delivered';
  if(kind==='read')return 'read';
  throw Error('untrusted receipt type');
 };
 const policy=Object.freeze({receiptTimeoutMs,markTxDone,encryptedReceiptStatus});
 root.OffGridLoRaDelivery=policy;
 if(typeof module!=='undefined'&&module.exports)module.exports=policy;
})(typeof globalThis!=='undefined'?globalThis:this);
