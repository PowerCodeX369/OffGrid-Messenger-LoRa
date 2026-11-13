/* SPDX-License-Identifier: GPL-3.0-or-later
 * OffGrid LoRa v1.7 RF adaptation policy, pure host-testable C.
 * This policy is an engineering starting point, NOT a distance estimator.
 * Reconfiguration is done exclusively through authenticated two-node handshake.
 */
#ifndef OFFGRID_LORA_AUTO_H
#define OFFGRID_LORA_AUTO_H
#include <stdint.h>
#include <stdbool.h>
#include "lora_modes.h"
#define OG_AUTO_METRIC_PERIOD_MS 3500u
#define OG_AUTO_METRIC_FRESH_MS 18000u
#define OG_AUTO_COOLDOWN_MS 45000u
#define OG_AUTO_PROMOTION_MS 22000u
#define OG_AUTO_DOWN_STREAK 2u
#define OG_AUTO_UP_STREAK 4u
/* Datasheet typ. sensitivities: SF7/BW500 -118 dBm, SF7/BW250 -122,
 * SF9/BW125 -131. Thresholds carry 10-20 dB reserve for real-world fading.
 * In automatic operation BALANCED TX is promoted to nominal 18 dBm; otherwise
 * its manual +10 dBm would be WEAKER than Fast +18 in link-budget terms.
 * User MUST confirm both module supplies/antennas before AUTO ON.
 */
#define OG_AUTO_FAST_DOWN_RSSI (-106)
#define OG_AUTO_FAST_UP_RSSI (-97)
#define OG_AUTO_BAL_DOWN_RSSI (-113)
#define OG_AUTO_BAL_UP_RSSI (-104)
#define OG_AUTO_FAST_DOWN_SNR_Q4 (-4)   /* -1 dB */
#define OG_AUTO_FAST_UP_SNR_Q4 (12)    /* +3 dB */
#define OG_AUTO_BAL_DOWN_SNR_Q4 (-12) /* -3 dB */
#define OG_AUTO_BAL_UP_SNR_Q4 (8)    /* +2 dB */

typedef struct {
    int16_t rssi_dbm;
    int16_t snr_q4;
    uint8_t samples;
    uint32_t at_ms;
} og_auto_quality_t;
typedef struct {
    og_auto_quality_t local, remote;
    uint8_t down_streak, up_streak;
    uint32_t started_ms, changed_ms, last_decision_ms;
    uint32_t last_local_crc, last_local_retx;
    bool sample_counters_ready;
} og_auto_policy_t;
static inline void og_auto_reset(og_auto_policy_t *p, uint32_t now_ms) {
    if(!p)return;
    *p=(og_auto_policy_t){0};p->started_ms=now_ms;
    p->changed_ms=now_ms;
}
static inline void og_auto_quality_note(og_auto_quality_t *q,int16_t rssi,int16_t snr_q4,uint32_t now){
    if(!q || rssi < -155 || rssi > -10 || snr_q4 < -95 || snr_q4 > 95)return;
    if(!q->samples || (uint32_t)(now-q->at_ms)>OG_AUTO_METRIC_FRESH_MS){
        q->rssi_dbm=rssi;q->snr_q4=snr_q4;q->samples=1;
    }else{
        q->rssi_dbm=(int16_t)((3*(int32_t)q->rssi_dbm+rssi)/4);
        q->snr_q4=(int16_t)((3*(int32_t)q->snr_q4+snr_q4)/4);
        if(q->samples<255)q->samples++;
    }
    q->at_ms=now;
}
static inline bool og_auto_fresh(const og_auto_quality_t *q,uint32_t now){
    return q && q->samples>=3 && (uint32_t)(now-q->at_ms)<OG_AUTO_METRIC_FRESH_MS;
}
/* Only the elected coordinator evaluates the decision. Both directions must
 * have recent authenticated measurements; use the WORSE direction. */
static inline uint8_t og_auto_decide(og_auto_policy_t *p,uint8_t current,
                              uint32_t now,bool has_traffic,bool rf_loss){
    if(!p || has_traffic || !og_auto_fresh(&p->local,now) ||
       !og_auto_fresh(&p->remote,now))return current;
    if((uint32_t)(now-p->last_decision_ms)<OG_AUTO_METRIC_PERIOD_MS)return current;
    const int16_t rssi=p->local.rssi_dbm<p->remote.rssi_dbm?p->local.rssi_dbm:p->remote.rssi_dbm;
    const int16_t snr=p->local.snr_q4<p->remote.snr_q4?p->local.snr_q4:p->remote.snr_q4;
    /* Under sharply degrading RF conditions, do not stay stuck on a
     * faster mode during a normal 45-second anti-flap cooldown. Still
     * require two separate decisions on TWO authenticated measurements. */
    const bool urgent_down=(current==OG_MODE_LOW && (rssi<=-113 || snr<=-16)) ||
                           (current==OG_MODE_BALANCED && (rssi<=-119 || snr<=-20));
    if((uint32_t)(now-p->changed_ms)<OG_AUTO_COOLDOWN_MS && !urgent_down)return current;
    p->last_decision_ms=now;
    bool down=false,up=false;
    switch(current){
        case OG_MODE_LOW:
            down=rf_loss || rssi<=OG_AUTO_FAST_DOWN_RSSI || snr<=OG_AUTO_FAST_DOWN_SNR_Q4;
            break;
        case OG_MODE_BALANCED:
            down=rf_loss || rssi<=OG_AUTO_BAL_DOWN_RSSI || snr<=OG_AUTO_BAL_DOWN_SNR_Q4;
            up=rssi>=OG_AUTO_FAST_UP_RSSI && snr>=OG_AUTO_FAST_UP_SNR_Q4;
            break;
        case OG_MODE_MAX:
            up=rssi>=OG_AUTO_BAL_UP_RSSI && snr>=OG_AUTO_BAL_UP_SNR_Q4;
            break;
        default:return current;
    }
    p->down_streak=down && p->down_streak<255?p->down_streak+1:down?p->down_streak:0;
    p->up_streak=up && p->up_streak<255?p->up_streak+1:up?p->up_streak:0;
    if(p->down_streak>=OG_AUTO_DOWN_STREAK){
        p->down_streak=p->up_streak=0;
        return current==OG_MODE_LOW?OG_MODE_BALANCED:OG_MODE_MAX;
    }
    if(p->up_streak>=OG_AUTO_UP_STREAK &&
       (uint32_t)(now-p->started_ms)>=OG_AUTO_PROMOTION_MS){
        p->down_streak=p->up_streak=0;
        return current==OG_MODE_MAX?OG_MODE_BALANCED:OG_MODE_LOW;
    }
    return current;
}
#endif
