'use strict';
/* Authenticated receive guard tests: retransmission, concurrency, rejection,
 * resource limits and finite replay window. No RF hardware is simulated. */
const assert=require('node:assert/strict');
const {AuthenticatedRxGuard}=require('../../web/lora_rx_guard.js');
let now=1000;
const g=new AuthenticatedRxGuard(2,3,100,()=>now);
assert.equal(g.state('ARQ:peer:1'),'new');
const first=g.begin('ARQ:peer:1');
assert(first);assert.equal(g.state('ARQ:peer:1'),'pending');
assert.equal(g.begin('ARQ:peer:1'),null);
g.note('ARQ:peer:1',11);g.note('ARQ:peer:1',12);g.note('ARQ:peer:1',11);
assert.deepEqual([...first.packetIds],[11,12]);
assert(g.accept(first));assert.equal(g.state('ARQ:peer:1'),'accepted');
assert.equal(g.begin('ARQ:peer:1'),null,'processed ciphertext must not run twice');
const failed=g.begin('ARQ:peer:2');assert(failed);
assert(g.reject(failed));assert.equal(g.state('ARQ:peer:2'),'new','bad HMAC/GCM must never commit');
const pending=g.begin('ARQ:peer:2'),full=g.begin('ARQ:peer:3');
assert(pending&&full);assert.equal(g.begin('ARQ:peer:4'),null,'validation queue bounded');
g.reject(pending);g.accept(full);
assert.equal(g.state('ARQ:peer:3'),'accepted');
now+=101;assert.equal(g.state('ARQ:peer:1'),'new','expired transfer IDs are forgotten');
for(let i=0;i<5;i++){const t=g.begin('key'+i);assert(t);assert(g.accept(t));}
assert(g.recent.size<=3);
assert(!g.accept(first),'stale token cannot mutate state');
assert(!g.reject(first),'stale token cannot evict another transfer');
g.clear();assert.equal(g.pending.size,0);assert.equal(g.recent.size,0);
// Deterministic stress exercises 100,000 replay, rejection, and expiry paths.
const s=new AuthenticatedRxGuard(8,128,1000,()=>now);
let x=0x9E3779B9;
for(let i=0;i<100000;i++){
 x^=x<<13;x^=x>>>17;x^=x<<5;
 const key='s:'+((x>>>0)%500),v=(x>>>0)%7;
 if(v===0)now+=25;
 const ticket=s.begin(key);
 if(ticket){if(v===1||v===3)s.reject(ticket);else s.accept(ticket);}
 s.prune();assert(s.pending.size<=8&&s.recent.size<=128);
}
console.log('PASS: authenticated RX guard dedup, pending validation, retry after failure, expiry, bounded 100000-op stress');
