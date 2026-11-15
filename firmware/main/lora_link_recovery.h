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
#define OG_RECOVERY_AUTO_DEFER_MS 15000u

typedef enum { OG_RECOVERY_NONE=0, OG_RECOVERY_ENTER_BALANCED,
               OG_RECOVERY_RETURN_HOME, OG_RECOVERY_LOCK_BALANCED } og_recovery_action_t;
typedef struct {
    bool probing;
    uint8_t home;
    uint32_t scan_until_ms;
    uint32_t next_scan_ms;
    uint32_t attempts;
} og_recovery_t;
static inline void og_recovery_reset(og_recovery_t *r){if(r)*r=(og_recovery_t){0};}
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
