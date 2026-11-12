/* Pure browser delivery-policy tests; no DOM needed. */
'use strict';
const assert=require('node:assert/strict');
const L=require('../../web/lora_delivery.js');
const P=require('../../web/protocol.js');
for(const n of [1,2,3,4,13]){
 const job={totalFrames:n,transmitted:new Set()};
 for(let i=0;i<n;i++)assert.equal(L.markTxDone(job,i),i===n-1);
 assert.equal(L.markTxDone(job,n-1),true); // duplicate TxDone does not un-send
 assert(L.receiptTimeoutMs(n)>=8500);
}
assert.throws(()=>L.markTxDone({totalFrames:1},2));
assert.throws(()=>L.receiptTimeoutMs(0));
assert.equal(L.encryptedReceiptStatus('delivered'),'delivered');
assert.equal(L.encryptedReceiptStatus('read'),'read');
assert.throws(()=>L.encryptedReceiptStatus('radioack'));
/* OGM4 500 Unicode characters must fit the same frame protocol without
 * relying on a 6-packet firmware window before decrypt completion. */
const msg='💬'.repeat(500);const blob=P.textBytes(msg);
assert.equal(blob.length,2000);
const framed=P.frames(P.newId(),new Uint8Array(blob.length+100));
assert(framed.length>8);
console.log('PASS: TxDone single-tick policy, authenticated receipt gates, receipt deadlines, 500-Unicode fragmentation');
