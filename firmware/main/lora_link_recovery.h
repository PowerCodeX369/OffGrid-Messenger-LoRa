/* SPDX-License-Identifier: GPL-3.0-or-later
 * v3.0.6 authenticated link rendezvous, pure C and host-testable.
 * A node on Range/Ultra that has not heard its peer explores the common
 * SF7/BW250 profile temporarily, without changing keys or acknowledging
 * application messages. It stays there ONLY after receiving an authenticated
 * peer frame, otherwise it returns to its previous profile and tries later.
 * Both nodes probe >50% of each cycle to guarantee an overlapping listening
 * window if both enter discovery, even with independent clocks.
 */
#ifndef OFFGRID_LORA_LINK_RECOVERY_H
#define OFFGRID_LORA_LINK_RECOVERY_H
#include <stdint.h>
#include <stdbool.h>
#include "lora_modes.h"
#define OG_RECOVERY_NORMAL_LOSS_MS 45000u
#define OG_RECOVERY_ULTRA_LOSS_MS 80000u
#define OG_RECOVERY_SCAN_MS 15000u
#define OG_RECOVERY_HOME_MS 10000u
#define OG_RECOVERY_BEACON_MS 2500u
#define OG_RECOVERY_ULTRA_BEACON_MS 7500u /* leave SF12 RX airtime between probes */
#define OG_RECOVERY_AUTO_DEFER_MS 15000u
/* Auto mode: after a true loss (RSSI cannot be measured), explore stronger
 * PHYs as well as the common profile. Short scans cannot guarantee discovery
 * across asynchronous clocks; repeated overlapping windows are best-effort. */
#define OG_AUTO_RECOVERY_BAL_LOSS_MS 16000u
#define OG_AUTO_RECOVERY_MAX_LOSS_MS 28000u
#define OG_AUTO_RECOVERY_ULTRA_LOSS_MS 65000u
#define OG_AUTO_RECOVERY_MAX_SCAN_MS 18000u
#define OG_AUTO_RECOVERY_ULTRA_SCAN_MS 30000u

typedef enum { OG_RECOVERY_NONE=0, OG_RECOVERY_ENTER_BALANCED,
               OG_RECOVERY_RETURN_HOME, OG_RECOVERY_LOCK_BALANCED,
               OG_RECOVERY_SCAN_PROFILE } og_recovery_action_t;
typedef struct {
    bool probing;
    bool multi_mode;
    uint8_t home, scan_mode, scan_stage;
    bool ultra_allowed;
    uint32_t scan_until_ms;
    uint32_t next_scan_ms;
    uint32_t attempts;
} og_recovery_t;
static inline void og_recovery_reset(og_recovery_t *r){if(r)*r=(og_recovery_t){0};}
/* Auto search may start from BALANCED itself (the legacy recovery never
 * did). Each probe is temporary; an authenticated RF frame is required to
 * commit it. The second scan searches the other plausible peer band. */
static inline uint8_t og_recovery_auto_scan(uint8_t home,uint8_t stage,bool ultra){
    /* Fast may be active only as a temporary media boost. A broken session
     * must still be able to rendezvous with the normal three-band radios. */
    if(home==OG_MODE_LOW){
        return stage==0u?OG_MODE_BALANCED:OG_MODE_MAX;
    }
    if(home==OG_MODE_BALANCED){
        return stage==0u?OG_MODE_MAX:(ultra?OG_MODE_ULTRA:OG_MODE_BALANCED);
    }
    if(home==OG_MODE_MAX){
        return stage==0u?(ultra?OG_MODE_ULTRA:OG_MODE_BALANCED):OG_MODE_BALANCED;
    }
    return stage==0u?OG_MODE_MAX:OG_MODE_BALANCED;
}
static inline uint32_t og_recovery_auto_dwell(uint8_t mode){
    return mode==OG_MODE_ULTRA?OG_AUTO_RECOVERY_ULTRA_SCAN_MS:
           OG_AUTO_RECOVERY_MAX_SCAN_MS;
}
static inline og_recovery_action_t og_recovery_step_auto(og_recovery_t *r,
       uint32_t now,uint32_t peer_age,uint8_t mode,bool blocked,
       bool authenticated_rx,bool ultra_capable){
    if(!r)return OG_RECOVERY_NONE;
    if(r->probing){
        if(authenticated_rx){
            r->probing=false;r->multi_mode=false;r->home=0;
            r->next_scan_ms=now+OG_RECOVERY_AUTO_DEFER_MS;
            return OG_RECOVERY_LOCK_BALANCED;
        }
        if((int32_t)(now-r->scan_until_ms)<0)return OG_RECOVERY_NONE;
        if(r->scan_stage==0u){
            uint8_t second=og_recovery_auto_scan(r->home,1u,r->ultra_allowed);
            if(second!=r->home && second!=r->scan_mode){
                r->scan_stage=1u;r->scan_mode=second;
                r->scan_until_ms=now+og_recovery_auto_dwell(second);
                return OG_RECOVERY_SCAN_PROFILE;
            }
        }
        r->probing=false;r->multi_mode=false;
        r->next_scan_ms=now+OG_RECOVERY_HOME_MS+(r->attempts%4u)*1137u;
        return OG_RECOVERY_RETURN_HOME;
    }
    const uint32_t loss=mode==OG_MODE_ULTRA?OG_AUTO_RECOVERY_ULTRA_LOSS_MS:
        mode==OG_MODE_MAX?OG_AUTO_RECOVERY_MAX_LOSS_MS:
        OG_AUTO_RECOVERY_BAL_LOSS_MS;
    if(blocked || peer_age<loss ||
       (r->next_scan_ms && (int32_t)(now-r->next_scan_ms)<0))return OG_RECOVERY_NONE;
    if(mode!=OG_MODE_LOW && mode!=OG_MODE_BALANCED &&
       mode!=OG_MODE_MAX && mode!=OG_MODE_ULTRA)
        return OG_RECOVERY_NONE;
    r->home=mode;r->multi_mode=true;r->ultra_allowed=ultra_capable;
    r->scan_stage=0u;r->scan_mode=og_recovery_auto_scan(mode,0u,ultra_capable);
    r->probing=true;r->attempts++;
    r->scan_until_ms=now+og_recovery_auto_dwell(r->scan_mode);
    return OG_RECOVERY_SCAN_PROFILE;
}
static inline og_recovery_action_t og_recovery_step(og_recovery_t *r,
        uint32_t now,uint32_t peer_age_ms,uint8_t mode,
        bool blocked,bool authenticated_rx){
    if(!r)return OG_RECOVERY_NONE;
    if(r->probing){
        if(authenticated_rx){r->probing=false;r->home=0;
            r->next_scan_ms=now+OG_RECOVERY_AUTO_DEFER_MS;
            return OG_RECOVERY_LOCK_BALANCED;}
        if((int32_t)(now-r->scan_until_ms)>=0){r->probing=false;
            r->next_scan_ms=now+OG_RECOVERY_HOME_MS+(r->attempts%4u)*1137u;
            return OG_RECOVERY_RETURN_HOME;}
        return OG_RECOVERY_NONE;
    }
    const uint32_t loss_threshold=mode==OG_MODE_ULTRA?
        OG_RECOVERY_ULTRA_LOSS_MS:OG_RECOVERY_NORMAL_LOSS_MS;
    if(mode==OG_MODE_BALANCED || blocked || peer_age_ms<loss_threshold ||
       (r->next_scan_ms && (int32_t)(now-r->next_scan_ms)<0))return OG_RECOVERY_NONE;
    r->home=mode;r->probing=true;r->scan_until_ms=now+OG_RECOVERY_SCAN_MS;
    r->attempts++;
    return OG_RECOVERY_ENTER_BALANCED;
}
#endif
