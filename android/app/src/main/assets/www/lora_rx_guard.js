/* v1.10 authenticated receive/replay guard.
 * Encrypted packets are accepted only after the caller finishes AES-GCM,
 * envelope validation and application handling. A duplicate in flight may
 * add legacy APPACK IDs, but must never run a second concurrent handler.
 * No persistent anti-replay claim: rebooted firmware control frames need
 * a separate, persistent on-device counter protocol. */
(function(root){'use strict';
 class AuthenticatedRxGuard {
  constructor(maxPending=16,maxRecent=512,ttlMs=12*60*1000,clock=()=>Date.now()){
   if(!Number.isInteger(maxPending)||maxPending<1||!Number.isInteger(maxRecent)||maxRecent<1||
      !Number.isFinite(ttlMs)||ttlMs<=0)throw Error('Invalid RX guard limits');
   this.maxPending=maxPending;this.maxRecent=maxRecent;this.ttlMs=ttlMs;this.clock=clock;
   this.pending=new Map();this.recent=new Map();
  }
  prune(){const now=this.clock();
   for(const [key,at] of this.recent)if(now-at>=this.ttlMs)this.recent.delete(key);
   while(this.recent.size>this.maxRecent)this.recent.delete(this.recent.keys().next().value);
  }
  state(key){this.prune();return this.recent.has(key)?'accepted':this.pending.has(key)?'pending':'new';}
  note(key,packetId){const ticket=this.pending.get(key);if(ticket&&packetId!==undefined&&packetId!==null)ticket.packetIds.add(packetId);}
  begin(key,packetIds=[]){this.prune();if(typeof key!=='string'||!key||this.recent.has(key)||this.pending.has(key)||this.pending.size>=this.maxPending)return null;
   const ticket={key,packetIds:new Set(packetIds)};this.pending.set(key,ticket);return ticket;
  }
  accept(ticket){if(this.pending.get(ticket?.key)!==ticket)return false;
   this.pending.delete(ticket.key);this.recent.set(ticket.key,this.clock());this.prune();return true;
  }
  reject(ticket){if(this.pending.get(ticket?.key)!==ticket)return false;this.pending.delete(ticket.key);return true;}
  clear(){this.pending.clear();this.recent.clear();}
 }
 const api=Object.freeze({AuthenticatedRxGuard});root.OffGridLoRaRxGuard=api;
 if(typeof module!=='undefined'&&module.exports)module.exports=api;
})(typeof globalThis!=='undefined'?globalThis:this);
