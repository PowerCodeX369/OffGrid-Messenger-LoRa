/* SPDX-License-Identifier: GPL-3.0-or-later
 * v3.0.6 automatic RSSI bands, shared by USB, BLE and the radio worker.
 * RSSI belongs to authenticated LoRa reception, NOT Bluetooth signal strength.
 * These are exact inclusive boundaries; no SNR override, smoothing or
 * promotion cooldown shifts the operator-selected band thresholds.
 * A profile may change only via the existing authenticated OFFER/AGREE/verify.
 */
#ifndef OFFGRID_LORA_AUTO_H
#define OFFGRID_LORA_AUTO_H
#include <stdint.h>
#include <stdbool.h>
#include "lora_modes.h"
#define OG_AUTO_METRIC_PERIOD_MS 3500u
#define OG_AUTO_METRIC_FRESH_MS 90000u /* retained for diagnostic/legacy use */
#define OG_AUTO_COOLDOWN_MS 45000u /* negotiated-failure defer; not RSSI hysteresis */
#define OG_AUTO_PROMOTION_MS 22000u /* legacy diagnostic compatibility */
#define OG_AUTO_DOWN_STREAK 1u
#define OG_AUTO_UP_STREAK 1u
#define OG_AUTO_BALANCED_LIMIT_DBM (-100)
#define OG_AUTO_ULTRA_LIMIT_DBM (-130)
/* Kept for older media/radio-test contracts; the AUTO decision does not
 * use these historical Fast/SNR thresholds anymore. */
#define OG_AUTO_FAST_DOWN_RSSI (-84)
#define OG_AUTO_FAST_RANGE_RSSI (-88)
#define OG_AUTO_FAST_UP_RSSI (-74)
#define OG_AUTO_BAL_DOWN_RSSI OG_AUTO_BALANCED_LIMIT_DBM
#define OG_AUTO_BAL_UP_RSSI OG_AUTO_BALANCED_LIMIT_DBM
#define OG_AUTO_FAST_DOWN_SNR_Q4 (0)
#define OG_AUTO_FAST_UP_SNR_Q4 (16)
#define OG_AUTO_BAL_DOWN_SNR_Q4 (-8)
#define OG_AUTO_BAL_UP_SNR_Q4 (12)
#define OG_AUTO_LOCAL_FRESH_MS 30000u
#define OG_AUTO_REMOTE_FRESH_MS 20000u
#define OG_AUTO_ULTRA_LOCAL_FRESH_MS 60000u

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
static inline void og_auto_reset(og_auto_policy_t *p,uint32_t now){
    if(!p)return;
    *p=(og_auto_policy_t){0};p->started_ms=now;p->changed_ms=now;
}
static inline void og_auto_quality_note(og_auto_quality_t *q,int16_t rssi,int16_t snr_q4,uint32_t now){
    if(!q || rssi < -155 || rssi > -10 || snr_q4 < -128 || snr_q4 > 127)return;
    /* Latest authenticated sample: EWMA masked a newly crossed -100/-130. */
    q->rssi_dbm=rssi;q->snr_q4=snr_q4;
    if(q->samples<255u)q->samples++;
    q->at_ms=now;
}
static inline bool og_auto_fresh(const og_auto_quality_t *q,uint32_t now){
    return q && q->samples>0u && (uint32_t)(now-q->at_ms)<=OG_AUTO_METRIC_FRESH_MS;
}
static inline bool og_auto_recent(const og_auto_quality_t *q,uint32_t now,uint32_t limit){
    return q && q->samples>0u && (uint32_t)(now-q->at_ms)<=limit;
}
static inline uint8_t og_auto_rssi_band(int16_t rssi){
    if(rssi>OG_AUTO_BALANCED_LIMIT_DBM)return OG_MODE_BALANCED;
    if(rssi>=OG_AUTO_ULTRA_LIMIT_DBM)return OG_MODE_MAX;
    return OG_MODE_ULTRA;
}
static inline uint8_t og_auto_decide(og_auto_policy_t *p,uint8_t current,
                                  uint32_t now,bool has_traffic,bool rf_loss){
    (void)rf_loss;
    if(!p||has_traffic||!og_auto_recent(&p->local,now,
       current==OG_MODE_ULTRA?OG_AUTO_ULTRA_LOCAL_FRESH_MS:OG_AUTO_LOCAL_FRESH_MS))return current;
    /* Use the worse direction only when the authenticated peer report is
     * recent. Missing peer metrics cannot prevent a locally observed drop.
     * A lost packet has NO RSSI; it is handled by recovery, not as -131. */
    int16_t rssi=p->local.rssi_dbm;
    if(og_auto_recent(&p->remote,now,OG_AUTO_REMOTE_FRESH_MS) &&
       p->remote.rssi_dbm<rssi)rssi=p->remote.rssi_dbm;
    return og_auto_rssi_band(rssi);
}
/* Peer consent is required, even when only the local authenticated RSSI is
 * available. This function does not command a unilateral PHY change. */
static inline uint8_t og_auto_decide_linkaware(og_auto_policy_t *p,uint8_t current,
       uint32_t now,bool busy,bool rf_loss,bool authenticated_peer_auto){
    if(!authenticated_peer_auto)return current;
    return og_auto_decide(p,current,now,busy,rf_loss);
}
#endif
