#include <assert.h>
#include <stdio.h>
#include "lora_link_recovery.h"
#include "lora_auto.h"
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
    /* Automatic mode searches *outward* after a true Balanced loss. */
    og_recovery_reset(&r);
    assert(og_recovery_step_auto(&r,15000,15000,OG_MODE_BALANCED,false,false,true)==OG_RECOVERY_NONE);
    assert(og_recovery_step_auto(&r,17000,17000,OG_MODE_BALANCED,false,false,true)==OG_RECOVERY_SCAN_PROFILE);
    assert(r.probing && r.home==OG_MODE_BALANCED && r.scan_mode==OG_MODE_MAX);
    assert(og_recovery_step_auto(&r,r.scan_until_ms-1,18000,OG_MODE_MAX,false,false,true)==OG_RECOVERY_NONE);
    assert(og_recovery_step_auto(&r,r.scan_until_ms+1,18000,OG_MODE_MAX,false,false,true)==OG_RECOVERY_SCAN_PROFILE);
    assert(r.scan_mode==OG_MODE_ULTRA);
    assert(og_recovery_step_auto(&r,r.scan_until_ms-1,19000,OG_MODE_ULTRA,false,true,true)==OG_RECOVERY_LOCK_BALANCED);
    assert(!r.probing && r.home==0);
    /* Cannot lock without an authenticated RF packet. */
    og_recovery_reset(&r);
    assert(og_recovery_step_auto(&r,29000,29000,OG_MODE_MAX,false,false,true)==OG_RECOVERY_SCAN_PROFILE);
    assert(r.scan_mode==OG_MODE_ULTRA);
    uint32_t next=r.scan_until_ms+1;
    assert(og_recovery_step_auto(&r,next,40000,OG_MODE_ULTRA,false,false,true)==OG_RECOVERY_SCAN_PROFILE);
    assert(r.scan_mode==OG_MODE_BALANCED);
    next=r.scan_until_ms+1;
    assert(og_recovery_step_auto(&r,next,40000,OG_MODE_BALANCED,false,false,true)==OG_RECOVERY_RETURN_HOME);
    assert(!r.probing && r.home==OG_MODE_MAX);
    /* Auto scan refuses an unsupported Ultra on a legacy peer. */
    og_recovery_reset(&r);
    assert(og_recovery_step_auto(&r,17000,17000,OG_MODE_BALANCED,false,false,false)==OG_RECOVERY_SCAN_PROFILE);
    assert(r.scan_mode==OG_MODE_MAX);
    assert(og_recovery_step_auto(&r,r.scan_until_ms+1,19000,OG_MODE_MAX,false,false,false)==OG_RECOVERY_RETURN_HOME);
    /* A temporary Fast media boost must not make automatic lost-link
     * recovery impossible when the phone or far-side radio disappears. */
    og_recovery_reset(&r);
    assert(og_recovery_step_auto(&r,17000,17000,OG_MODE_LOW,false,false,true)==OG_RECOVERY_SCAN_PROFILE);
    assert(r.home==OG_MODE_LOW && r.scan_mode==OG_MODE_BALANCED);
    assert(og_recovery_step_auto(&r,r.scan_until_ms+1,17000,OG_MODE_BALANCED,false,false,true)==OG_RECOVERY_SCAN_PROFILE);
    assert(r.scan_mode==OG_MODE_MAX);
    assert(OG_AUTO_RECOVERY_BAL_LOSS_MS<OG_RECOVERY_NORMAL_LOSS_MS);
    puts("PASS: manual legacy rendezvous + Auto Balanced->Max->Ultra / reverse search; authenticated-only lock and return-home");
}
