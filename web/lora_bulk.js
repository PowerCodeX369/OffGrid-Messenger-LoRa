/* OffGrid v1.6 authenticated transfer bulk USB client.
 * The browser encrypts the whole OGM4 envelope before staging ciphertext.
 * CRC32 protects USB staging, not application secrecy/authenticity. */
(function(root){'use strict';
 const MAX=7168,CHUNK=160,PIPELINE=8;
 const HEX=/^[0-9A-F]{16}$/;
 const TYPE=/^(BULKREADY|BULKSTEP|BULKQUEUED|BULKSENT|BULKPROGRESS|BULKCONFIRMED|BULKFAILED|BULKSTAT|RXBSTART|RXBCHUNK|RXBDONE)\|/;
 const asHex=a=>Array.from(a,b=>b.toString(16).padStart(2,'0')).join('').toUpperCase();
 const fromHex=s=>{if(typeof s!=='string'||!/^(?:[0-9a-f]{2})+$/i.test(s))throw Error('Invalid bulk hex');return Uint8Array.from(s.match(/../g),h=>parseInt(h,16));};
 function crc32(bytes){let c=0xFFFFFFFF;for(const b of bytes){c^=b;for(let i=0;i<8;i++)c=(c>>>1)^((c&1)?0xEDB88320:0);}return (c^0xFFFFFFFF)>>>0;}
 const hex32=n=>(n>>>0).toString(16).toUpperCase().padStart(8,'0');
 function waitResult(t,predicate,timeout=4500){
  return new Promise((resolve,reject)=>{
   if(t.waiter){reject(Error('Bulk protocol in-flight stage waiter'));return;}
   const timer=setTimeout(()=>{if(t.waiter?.timer===timer)t.waiter=null;reject(Error('LoRa USB bulk stage timed out'));},timeout);
   t.waiter={predicate,resolve:value=>{clearTimeout(timer);resolve(value);},reject:e=>{clearTimeout(timer);reject(e);},timer};
  });
 }
 class BulkLink{
  constructor(write,onReceive){this.write=write;this.onReceive=onReceive;this.active=null;this.rx=null;this.closed=false;this.stats={confirmed:0,failed:0,rx:0};}
  cancel(reason='USB disconnected'){
   this.closed=true;
   if(this.active){const a=this.active;this.active=null;if(a.waiter){a.waiter.reject(Error(reason));a.waiter=null;}a.rejectFinal(Error(reason));}
   this.rx=null;
  }
  async send(id,ciphertext,priority,{onSent=()=>{},onProgress=()=>{}}={}){
   id=String(id).toUpperCase();
   if(this.closed||this.active)throw Error('LoRa bulk connection busy or disconnected');
   if(!HEX.test(id)||id.slice(0,8)==='00000000'||id.slice(8)==='00000000'||
      !(ciphertext instanceof Uint8Array)||ciphertext.length<28||ciphertext.length>MAX)
    throw Error('Invalid encrypted LoRa bulk transfer');
   const a={id,priority,waiter:null,onSent,onProgress,sent:false,done:false,resolveFinal:null,rejectFinal:null,started:performance.now(),stagedAt:0,radioStats:null};
   const final=new Promise((resolve,reject)=>{a.resolveFinal=resolve;a.rejectFinal=reject;});
   /* A rejected terminal promise can arrive while stage writes are pending.
    * Attach a handler immediately so browsers do not report unhandled rejects. */
   final.catch(()=>{});
   this.active=a;
   const sendStage=async(line,kind,seq)=>{
    const expected=waitResult(a,(parts)=>parts[0]===kind&&(kind!=='BULKSTEP'||Number(parts[2])===seq));
    try{await this.write(line);await expected;}catch(e){
     if(a.waiter){a.waiter.reject(e);a.waiter=null;}
     throw e;
    }
   };
   try{
    await sendStage(`BULKBEGIN ${id} ${ciphertext.length} ${priority?'CTRL':'DATA'}`,'BULKREADY');
    /* Bounded 8-line USB write (< 3 KiB), below the firmware's 4-KiB IRQ RX buffer.
     * The radio task processes each chunk in
     * strict sequence and replies at four-line checkpoints. We await the
     * checkpoint for the final line of the 8-line browser batch.
     * A failed intermediate chunk emits BULKFAILED and rejects the waiter;
     * therefore none of the intermediate integrity checks is bypassed. */
    let seq=0;
    for(let off=0;off<ciphertext.length;){
     const lines=[];
     for(let n=0;n<PIPELINE&&off<ciphertext.length;n++,seq++,off+=CHUNK){
      lines.push(`BULKCHUNK ${id} ${seq} ${asHex(ciphertext.subarray(off,off+CHUNK))}`);
     }
     await sendStage(lines.join('\n'),'BULKSTEP',seq-1);
    }
    await sendStage(`BULKEND ${id} ${hex32(crc32(ciphertext))}`,'BULKQUEUED');
    a.stagedAt=performance.now();
    /* Timeout scales with Max/Range airtime and 32 possible fragments. */
    const ms=22000+Math.ceil(ciphertext.length/224)*4500;
    const timeout=setTimeout(()=>a.rejectFinal(Error('LoRa bitmap ACK transfer unconfirmed')),ms);
    try{await final;this.stats.confirmed++;}finally{clearTimeout(timeout);}
    return Object.freeze({usbStageMs:a.stagedAt-a.started,
      transferMs:performance.now()-a.started,radioWaitMs:performance.now()-a.stagedAt,
      ...(a.radioStats||{})});
   }catch(e){this.stats.failed++;throw e;}
   finally{if(this.active===a)this.active=null;}
  }
  onLine(raw){
   if(!TYPE.test(raw))return false;
   const parts=raw.split('|'),type=parts[0],id=(parts[1]||'').toUpperCase();
   if(type.startsWith('RXB')){
    try{
     if(type==='RXBSTART'){
      const n=Number(parts[2]);
      if(!HEX.test(id)||!Number.isInteger(n)||n<28||n>MAX)throw Error('Invalid RXBSTART');
      this.rx={id,size:n,bytes:new Uint8Array(n),seq:0,offset:0,mac:parts[3]||'UNKNOWN'};
     }else if(type==='RXBCHUNK'){
      const r=this.rx;const seq=Number(parts[2]);
      if(!r||id!==r.id||!Number.isInteger(seq)||r.seq!==seq)throw Error('Invalid RXBCHUNK order');
      const b=fromHex(parts[3]||'');
      if(b.length>CHUNK||r.offset+b.length>r.size)throw Error('Invalid RXBCHUNK size');
      r.bytes.set(b,r.offset);r.offset+=b.length;r.seq++;
     }else if(type==='RXBDONE'){
      const r=this.rx;this.rx=null;
      if(!r||r.id!==id||r.offset!==r.size||hex32(crc32(r.bytes))!==parts[2])throw Error('Invalid RXBDONE CRC');
      this.stats.rx++;
      Promise.resolve().then(()=>this.onReceive(r.mac,r.id,r.bytes)).catch(e=>console.warn('Encrypted LoRa bulk rejected',e));
     }
    }catch(e){console.warn('Dropped invalid LoRa bulk RX',e.message);this.rx=null;}
    return true;
   }
   const a=this.active;
   if(!a || (id!=='SESSION'&&id!==a.id))return true;
   if(type==='BULKSTAT'){
    const stats={};
    for(const field of parts.slice(2)){
     const cut=field.indexOf('=');if(cut<1)continue;
     const key=field.slice(0,cut),value=Number(field.slice(cut+1));
     if(['RF_MS','FRAMES','RETX','TIMEOUT'].includes(key)&&Number.isFinite(value)&&value>=0)
      stats[key]=value;
    }
    a.radioStats=stats;
   }else if(type==='BULKFAILED'){
    const err=Error('ESP32 bulk transfer unconfirmed: '+parts.slice(2).join(' '));
    if(a.waiter){a.waiter.reject(err);a.waiter=null;}
    a.rejectFinal(err);
   }else if(type==='BULKSENT'){
    if(!a.sent){a.sent=true;a.onSent();}
   }else if(type==='BULKPROGRESS'){
    const n=Number(parts[2]),total=Number(parts[3]);
    if(Number.isInteger(n)&&Number.isInteger(total)&&total>0)a.onProgress(n,total);
   }else if(type==='BULKCONFIRMED'){
    a.done=true;a.resolveFinal();
   }else if(a.waiter){
    const w=a.waiter;
    if(w.predicate(parts)){a.waiter=null;w.resolve(parts);}
   }
   return true;
  }
 }
 const api=Object.freeze({MAX,CHUNK,PIPELINE,crc32,hex32,BulkLink});root.OffGridLoRaBulk=api;
 if(typeof module!=='undefined'&&module.exports)module.exports=api;
})(typeof globalThis!=='undefined'?globalThis:this);
