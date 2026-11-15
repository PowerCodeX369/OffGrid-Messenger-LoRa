/* SPDX-License-Identifier: GPL-3.0-or-later
 * Transient retry scheduling ONLY. The original message payload and status
 * persist in authenticated/encrypted IndexedDB history in app.js.
 * Never treat Web Serial connected as LoRa peer reachable.
 */
(function(root){'use strict';
 function make(){
  const attempts=new Map(),dueAt=new Map();
  const delay=n=>Math.min(300000,15000*(2**Math.max(0,Math.min(n,6)-1)));
  // Message IDs are random. Stable 0-5s jitter avoids synchronized ACK-loss
  // retries without writing retry metadata to unencrypted browser storage.
  const jitter=id=>{let h=0;for(let i=0;i<id.length;i++)h=(Math.imul(31,h)+id.charCodeAt(i))|0;return (h&0x7fffffff)%5001;};
  return Object.freeze({
   due:(id,now,fresh)=>!!fresh&&(dueAt.get(id)??0)<=now,
   attempt:(id,now)=>{const n=Math.min(16,(attempts.get(id)||0)+1);attempts.set(id,n);dueAt.set(id,now+delay(n)+jitter(id));return n;},
   postpone:(id,now)=>dueAt.set(id,Math.max(dueAt.get(id)||0,now+delay(attempts.get(id)||1)+jitter(id))),
   holdForReceipt:(id,now,ultra)=>dueAt.set(id,Math.max(dueAt.get(id)||0,now+(ultra?240000:90000))),
   clear:id=>{attempts.delete(id);dueAt.delete(id);},
   reset:()=>{attempts.clear();dueAt.clear();},
   delay,jitter
  });
 }
 const api=Object.freeze({make});
 root.OffGridWeakTextQueue=api;
 if(typeof module!=='undefined'&&module.exports)module.exports=api;
})(typeof globalThis!=='undefined'?globalThis:this);
