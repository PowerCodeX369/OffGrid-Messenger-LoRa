/* Authenticated LoRa application-throughput results, not PHY bitrate. */
(function(root){'use strict';
 function summarize(results,elapsedMs){
  if(!Array.isArray(results)||!Number.isFinite(elapsedMs)||elapsedMs<=0)throw Error('Invalid benchmark results');
  const okResults=results.filter(r=>r.ok && Number.isFinite(r.ms) && r.ms>=0 && Number.isFinite(r.bytes) && r.bytes>=0);
  const times=okResults.map(r=>r.ms).sort((a,b)=>a-b);
  const quantile=q=>times.length?times[Math.min(times.length-1,Math.ceil(times.length*q)-1)]:null;
  const confirmed=okResults.reduce((sum,r)=>sum+r.bytes,0);
  return Object.freeze({total:results.length,ok:okResults.length,lost:results.length-okResults.length,
   lossPct:results.length?100*(results.length-okResults.length)/results.length:0,
   confirmedBytes:confirmed,elapsedMs,goodputBps:1000*confirmed/elapsedMs,
   medianMs:quantile(.5),p95Ms:quantile(.95)});
 }
 const api=Object.freeze({summarize});root.OffGridLoRaBenchmark=api;
 if(typeof module!=='undefined'&&module.exports)module.exports=api;
})(typeof globalThis!=='undefined'?globalThis:this);
