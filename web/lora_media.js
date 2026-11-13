/* OffGrid Messenger LoRa v1.8 media segmentation.
 * The browser encrypts each segment with a fresh AES-GCM nonce and transfer ID.
 * Only complete media passing SHA-256 and format checks becomes a chat message.
 * No bare picture/audio is ever handed to the SX1278/USB radio transport. */
(function(root){'use strict';
 const MAX_PHOTO=50*1024, MAX_VOICE=20*1024, SEGMENT=5800, HEADER=28, MAX_PARTS=10;
 const codeForMime=Object.freeze({'image/webp':0,'image/png':1,'image/jpeg':2,
   'audio/webm':0,'audio/ogg':1,'audio/mp4':2});
 const imageMimes=['image/webp','image/png','image/jpeg'];
 const voiceMimes=['audio/webm','audio/ogg','audio/mp4'];
 function check(kind,mime,bytes){
  if(kind!=='image'&&kind!=='voice')throw Error('Invalid media kind');
  if(!Object.hasOwn(codeForMime,mime))throw Error('Unsupported media codec');
  if(kind==='image'&&!imageMimes.includes(mime)||kind==='voice'&&!voiceMimes.includes(mime))throw Error('Wrong MIME for media type');
  if(!(bytes instanceof Uint8Array)||bytes.length<256||bytes.length>(kind==='image'?MAX_PHOTO:MAX_VOICE))throw Error('Media byte budget exceeded');
 }
 async function digest(bytes){return new Uint8Array(await crypto.subtle.digest('SHA-256',bytes)).slice(0,16);}
 async function split(kind,mime,bytes){
  check(kind,mime,bytes);
  const count=Math.ceil(bytes.length/SEGMENT);if(count>MAX_PARTS)throw Error('Too many media segments');
  const hash=await digest(bytes),parts=[];
  for(let i=0;i<count;i++){
   const piece=bytes.subarray(i*SEGMENT,Math.min(bytes.length,(i+1)*SEGMENT));
   const part=new Uint8Array(HEADER+piece.length),dv=new DataView(part.buffer);
   part[0]=1;part[1]=kind==='image'?2:7;part[2]=codeForMime[mime];part[3]=0;
   dv.setUint16(4,i,false);dv.setUint16(6,count,false);dv.setUint32(8,bytes.length,false);
   part.set(hash,12);part.set(piece,HEADER);parts.push(part);
  }
  return parts;
 }
 function parse(body){
  if(!(body instanceof Uint8Array)||body.length<HEADER+1||body.length>HEADER+SEGMENT||body[0]!==1||body[3]!==0)return null;
  const kind=body[1]===2?'image':body[1]===7?'voice':null;
  const mimes=kind==='image'?imageMimes:kind==='voice'?voiceMimes:[];
  if(!kind||!mimes[body[2]])return null;
  const dv=new DataView(body.buffer,body.byteOffset,body.byteLength),index=dv.getUint16(4,false),count=dv.getUint16(6,false),total=dv.getUint32(8,false);
  const maximum=kind==='image'?MAX_PHOTO:MAX_VOICE;
  if(!count||count>MAX_PARTS||count!==Math.ceil(total/SEGMENT)||index>=count||total<256||total>maximum)return null;
  const expected=Math.min(SEGMENT,total-index*SEGMENT);
  if(expected!==body.length-HEADER)return null;
  return {kind,mime:mimes[body[2]],index,count,total,hash:body.slice(12,28),bytes:body.slice(28)};
 }
 function sameBytes(a,b){if(a.length!==b.length)return false;let diff=0;for(let i=0;i<a.length;i++)diff|=a[i]^b[i];return diff===0;}
 class Reassembler{
  constructor(maxPending=3){this.maxPending=maxPending;this.inflight=new Map();}
  clear(){this.inflight.clear();}
  async ingest(sender,id,body,metadata={}){
   if(!/^[0-9A-F]{16}$/i.test(id)||typeof sender!=='string'||sender.length>80)throw Error('Invalid media sender or ID');
   const part=parse(body);if(!part)throw Error('Invalid encrypted media segment');
   const key=sender+'|'+id,now=Date.now();
   for(const [k,v] of this.inflight)if(now-v.updated>20*60*1000)this.inflight.delete(k);
   let state=this.inflight.get(key);
   if(!state){
    if(this.inflight.size>=this.maxPending) this.inflight.delete(this.inflight.keys().next().value);
    state={kind:part.kind,mime:part.mime,count:part.count,total:part.total,hash:part.hash,
      metadata:{...metadata},pieces:Array(part.count).fill(null),received:0,updated:now};this.inflight.set(key,state);
   }
   if(state.kind!==part.kind||state.mime!==part.mime||state.count!==part.count||state.total!==part.total||
      !sameBytes(state.hash,part.hash)||JSON.stringify(state.metadata)!==JSON.stringify(metadata))throw Error('Conflicting media segment');
   state.updated=now;
   if(state.pieces[part.index]){
    if(!sameBytes(state.pieces[part.index],part.bytes))throw Error('Conflicting duplicate media segment');
   }else{state.pieces[part.index]=part.bytes;state.received++;}
   if(state.received!==state.count)return {complete:false,received:state.received,total:state.count};
   const assembled=new Uint8Array(state.total);let pos=0;
   for(const piece of state.pieces){assembled.set(piece,pos);pos+=piece.length;}
   if(pos!==assembled.length)throw Error('Media size mismatch');
   const h=await digest(assembled);
   if(!sameBytes(h,state.hash)){this.inflight.delete(key);throw Error('Media SHA-256 mismatch');}
   this.inflight.delete(key);
   return {complete:true,kind:state.kind,mime:state.mime,bytes:assembled,metadata:state.metadata};
  }
 }
 const api=Object.freeze({MAX_PHOTO,MAX_VOICE,SEGMENT,HEADER,split,parse,Reassembler});
 root.OffGridLoRaMedia=api;
 if(typeof module!=='undefined'&&module.exports)module.exports=api;
})(typeof globalThis!=='undefined'?globalThis:this);
