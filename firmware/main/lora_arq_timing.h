/* SPDX-License-Identifier: GPL-3.0-or-later
 * SF9/SF12 selective-ACK scheduling. A non-END_BURST partial ACK must not
 * start in the middle of the NEXT LoRa packet. Only END_BURST ACKs use the
 * existing fast receive->transmit turnaround. No changes to Fast/Balanced.
 */
#ifndef OFFGRID_LORA_ARQ_TIMING_H
#define OFFGRID_LORA_ARQ_TIMING_H
#include <stdint.h>
#include "lora_modes.h"
#include "og_sr.h"
#include "ultra/og_ultra_policy.h"

static inline uint32_t og_lora_arq_silence_ms(uint8_t mode,uint8_t frag_payload){
    if(mode==OG_MODE_ULTRA || mode==OG_MODE_MAX){
        const uint8_t min_payload=mode==OG_MODE_ULTRA?48u:128u;
        const uint8_t max_payload=mode==OG_MODE_ULTRA?96u:224u;
        if(frag_payload<min_payload || frag_payload>max_payload)
            frag_payload=max_payload;
        const uint16_t n=(uint16_t)frag_payload+OG_SR_HEADER+OG_SR_TAG;
        const uint32_t airtime=og_ultra_airtime_ms(
            mode==OG_MODE_ULTRA?12u:9u,125000u,
            mode==OG_MODE_ULTRA?8u:5u,8u,n);
        /* 25-ms minimum inter-frame gap + scheduler and RX IRQ margin. */
        return airtime?airtime+180u:0u;
    }
    return mode==OG_MODE_BALANCED?480u:200u;
}
static inline uint32_t og_lora_arq_ack_rto_ms(uint8_t mode,unsigned attempt){
    if(mode==OG_MODE_ULTRA)
        return og_ultra_ack_wait_ms(OG_SR_TAG,(uint8_t)(attempt>5u?5u:attempt));
    if(mode==OG_MODE_MAX){
        const uint32_t air=og_ultra_airtime_ms(9,125000,5,8,OG_SR_TAG);
        return air+650u+(attempt>4u?4u:attempt)*120u;
    }
    return mode==OG_MODE_BALANCED?750u:420u;
}
#endif
