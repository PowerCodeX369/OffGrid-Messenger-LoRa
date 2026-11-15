/* SPDX-License-Identifier: GPL-3.0-or-later
 * Profile-verification time division for long-airtime SF12 control packets.
 * Both radios can start VERIFYING within milliseconds of each other after a
 * signed OFFER/AGREE. Simultaneous 2.8s HELLOs collide in half-duplex LoRa.
 * These ROLE-SPECIFIC slots prevent the deterministic collision. No new RF
 * packet type, encryption or ARQ wire format is introduced.
 */
#ifndef OG_PROFILE_VERIFY_TIMING_H
#define OG_PROFILE_VERIFY_TIMING_H
#include <stdint.h>
#include <stdbool.h>
#define OG_ULTRA_VERIFY_LEADER_FIRST_MS 180u
#define OG_ULTRA_VERIFY_FOLLOWER_FIRST_MS 4800u
#define OG_ULTRA_VERIFY_REPEAT_AFTER_TX_MS 7000u
#define OG_ULTRA_VERIFY_ASSIST_AFTER_RX_MS 380u
#define OG_ULTRA_VERIFY_MAX_MS 47000u
/* The leader (the one that sent PROFILE OFFER) emits the initial HELLO;
 * the follower listens first, then transmits if the HELLO was lost. */
static inline uint32_t og_verify_first_delay_ms(uint8_t mode,bool initiator){
    return mode==4u ? (initiator?OG_ULTRA_VERIFY_LEADER_FIRST_MS:
                                  OG_ULTRA_VERIFY_FOLLOWER_FIRST_MS):0u;
}
static inline uint32_t og_verify_repeat_after_tx_ms(uint8_t mode){
    return mode==4u ? OG_ULTRA_VERIFY_REPEAT_AFTER_TX_MS:650u;
}
static inline uint32_t og_verify_assist_after_rx_ms(uint8_t mode,bool initiator){
    return mode==4u ? (initiator?OG_ULTRA_VERIFY_REPEAT_AFTER_TX_MS:
                                 OG_ULTRA_VERIFY_ASSIST_AFTER_RX_MS):120u;
}
#endif
