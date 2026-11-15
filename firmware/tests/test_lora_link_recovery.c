#include <assert.h>
#include <stdio.h>
#include "lora_link_recovery.h"
#include "lora_auto.h"
static void quality(og_auto_quality_t *q,int r,int snr,uint32_t t){
    og_auto_quality_note(q,r,snr,t);
    og_auto_quality_note(q,r,snr,t+30);
}
int main(void){
    og_recovery_t r={0};
    /* Healthy link must NEVER change a profile just to seek a peer. */
    assert(og_recovery_step(&r,50000,1000,OG_MODE_ULTRA,false,false)==OG_RECOVERY_NONE);
    assert(og_recovery_step(&r,50000,50000,OG_MODE_ULTRA,false,false)==OG_RECOVERY_NONE);
    assert(og_recovery_step(&r,81000,81000,OG_MODE_ULTRA,false,false)==OG_RECOVERY_ENTER_BALANCED);
    assert(r.probing && r.home==OG_MODE_ULTRA);
    assert(og_recovery_step(&r,85000,85000,OG_MODE_BALANCED,false,false)==OG_RECOVERY_NONE);
    /* Authenticated peer discovery on fallback channel locks both to
     * Balanced; no unilateral claim until the actual HMAC is checked. */
    assert(og_recovery_step(&r,87000,0,OG_MODE_BALANCED,false,true)==OG_RECOVERY_LOCK_BALANCED);
    assert(!r.probing && r.home==0);
    assert(og_recovery_step(&r,100000,100,OG_MODE_BALANCED,false,false)==OG_RECOVERY_NONE);
    /* A missed scan returns to home (SF9 retained) and retries later. */
    og_recovery_reset(&r);
    assert(og_recovery_step(&r,70000,70000,OG_MODE_MAX,true,false)==OG_RECOVERY_NONE);
    assert(og_recovery_step(&r,70000,70000,OG_MODE_MAX,false,false)==OG_RECOVERY_ENTER_BALANCED);
    assert(og_recovery_step(&r,85001,85001,OG_MODE_BALANCED,false,false)==OG_RECOVERY_RETURN_HOME);
    assert(!r.probing && r.home==OG_MODE_MAX);
    assert(og_recovery_step(&r,85500,85500,OG_MODE_MAX,false,false)==OG_RECOVERY_NONE);
    assert(og_recovery_step(&r,r.next_scan_ms+1,95000,OG_MODE_MAX,false,false)==OG_RECOVERY_ENTER_BALANCED);
    /* A newly authenticated peer must be able to rescue a previous
     * mismatched profile even after two failed rendezvous windows. */
    assert(og_recovery_step(&r,r.scan_until_ms-1,0,OG_MODE_BALANCED,false,true)==OG_RECOVERY_LOCK_BALANCED);
    /* Never scan Balanced; it IS the safe/common rendezvous channel. */
    og_recovery_reset(&r);
    assert(og_recovery_step(&r,200000,200000,OG_MODE_BALANCED,false,false)==OG_RECOVERY_NONE);
    /* Auto fallback: remote quality missing, but peer explicitly enabled
     * Auto and local authenticated reception is degrading. Only DOWNGRADE.
     * Never infer a peer link or force an unsolicited radio change. */
    og_auto_policy_t p;og_auto_reset(&p,1000);
    quality(&p.local,-92,-4,2000);
    assert(og_auto_decide_linkaware(&p,OG_MODE_BALANCED,6000,false,false,false)==OG_MODE_BALANCED);
    assert(og_auto_decide_linkaware(&p,OG_MODE_BALANCED,6000,false,false,true)==OG_MODE_BALANCED);
    assert(og_auto_decide_linkaware(&p,OG_MODE_BALANCED,9800,false,false,true)==OG_MODE_MAX);
    og_auto_reset(&p,1000);
    quality(&p.local,-66,20,2000);
    assert(og_auto_decide_linkaware(&p,OG_MODE_BALANCED,9800,false,false,true)==OG_MODE_BALANCED);
    og_auto_reset(&p,1000);
    quality(&p.local,-116,-24,2000);
    assert(og_auto_decide_linkaware(&p,OG_MODE_MAX,6000,false,false,true)==OG_MODE_MAX);
    assert(og_auto_decide_linkaware(&p,OG_MODE_MAX,9800,false,false,true)==OG_MODE_ULTRA);
    og_auto_reset(&p,1000);
    quality(&p.local,-116,-24,2000);
    assert(og_auto_decide_linkaware(&p,OG_MODE_MAX,30000,false,false,true)==OG_MODE_MAX);/*stale*/
    assert(OG_RECOVERY_SCAN_MS>OG_RECOVERY_HOME_MS);
    puts("PASS: authenticated recovery rendezvous, keep SF9/Ultra profiles, short-range rescue, local-only safe Auto downgrade");
}
