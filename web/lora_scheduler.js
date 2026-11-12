/* OffGrid Messenger LoRa v1.2: bounded one-at-a-time browser->radio gate.
 * This is deliberately independent of DOM/Web Serial so collision, priority,
 * and reconnect behavior can be regression-tested with deterministic promises.
 * RF ACKs are still generated immediately by firmware (not by this gate).
 */
(function(root){'use strict';
 class FragmentScheduler {
  constructor(maxQueued=64){
   this.maxQueued=maxQueued;
   this.high=[];this.normal=[];this.active=false;
   this.closed=false;this.epoch=0;this.urgentBurst=0;
  }
  get depth(){return this.high.length+this.normal.length+(this.active?1:0);}
  submit(priority,work){
   if(this.closed)return Promise.reject(Error('Radio session closed'));
   if(priority!=='receipt'&&priority!=='data')return Promise.reject(Error('Invalid LoRa priority'));
   if(typeof work!=='function')return Promise.reject(Error('Invalid radio job'));
   if(this.depth>=this.maxQueued)return Promise.reject(Error('LoRa browser queue is full'));
   return new Promise((resolve,reject)=>{
    const item={work,resolve,reject,epoch:this.epoch};
    (priority==='receipt'?this.high:this.normal).push(item);
    void this._pump();
   });
  }
  async _pump(){
   if(this.active||this.closed)return;
   this.active=true;
   try{
    while(!this.closed&&(this.high.length||this.normal.length)){
     /* Receipts preempt at packet boundaries; after four urgent packets,
      * let one normal data packet run so neither direction can starve. */
     const urgent=this.high.length>0&&(!this.normal.length||this.urgentBurst<4);
     const item=urgent?this.high.shift():this.normal.shift();
     this.urgentBurst=urgent?this.urgentBurst+1:0;
     if(item.epoch!==this.epoch){item.reject(Error('Radio session changed'));continue;}
     try{const result=await item.work();item.resolve(result);}
     catch(e){item.reject(e);}
     await Promise.resolve();
    }
   }finally{this.active=false;}
  }
  cancelQueued(reason='Radio session closed'){
   this.epoch++;
   const err=Error(reason);
   for(const item of this.high.splice(0))item.reject(err);
   for(const item of this.normal.splice(0))item.reject(err);
   this.urgentBurst=0;
  }
  close(reason='Radio session closed'){this.closed=true;this.cancelQueued(reason);}
 }
 const api=Object.freeze({FragmentScheduler});
 root.OffGridLoRaScheduler=api;
 if(typeof module!=='undefined'&&module.exports)module.exports=api;
})(typeof globalThis!=='undefined'?globalThis:this);
