/* v1.2 authenticated-application receipt batching.
 * Each item is (kind=delivered/read, 8-byte OGM4 ID). Envelope is AES-GCM
 * encrypted by the existing OGM4 codec; no unauthenticated UI tick exists.
 * Eight receipts fit one 160-byte OGM4 radio fragment after encryption.
 */
(function(root){'use strict';
 const MAX_ITEMS=8;
 const kinds=Object.freeze({delivered:3,read:4});
 function pack(items){
  if(!Array.isArray(items)||!items.length||items.length>MAX_ITEMS)throw Error('Invalid receipt batch');
  const result=new Uint8Array(1+items.length*9);result[0]=items.length;
  const seen=new Set();
  items.forEach((item,i)=>{
   if(!item||!Object.hasOwn(kinds,item.kind)||typeof item.targetId!=='string'||
     !/^[0-9A-Fa-f]{16}$/.test(item.targetId))throw Error('Invalid receipt');
   const dedup=item.kind+':'+item.targetId.toUpperCase();
   if(seen.has(dedup))throw Error('Duplicate receipt in batch');seen.add(dedup);
   result[1+i*9]=kinds[item.kind];
   for(let j=0;j<8;j++)result[2+i*9+j]=parseInt(item.targetId.slice(j*2,j*2+2),16);
  });
  return result;
 }
 function unpack(data){
  if(!(data instanceof Uint8Array)||data.length<10||data[0]===0||data[0]>MAX_ITEMS||
     data.length!==1+data[0]*9)throw Error('Invalid receipt batch');
  const seen=new Set(),items=[];
  for(let i=0;i<data[0];i++){
   const type=data[1+i*9];if(type!==3&&type!==4)throw Error('Unknown receipt type');
   const kind=type===3?'delivered':'read';
   const targetId=Array.from(data.subarray(2+i*9,10+i*9),b=>b.toString(16).padStart(2,'0')).join('').toUpperCase();
   const key=kind+':'+targetId;if(seen.has(key))throw Error('Duplicate receipt');seen.add(key);
   items.push({kind,targetId});
  }
  return items;
 }
 const api=Object.freeze({MAX_ITEMS,pack,unpack});root.OffGridLoRaReceipts=api;
 if(typeof module!=='undefined'&&module.exports)module.exports=api;
})(typeof globalThis!=='undefined'?globalThis:this);
