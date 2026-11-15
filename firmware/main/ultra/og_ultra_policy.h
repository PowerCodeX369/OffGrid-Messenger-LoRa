/* OffGrid Ultra Long Range — portable policy, NOT a flashing firmware image.
 * Copyright 2026. SPDX-License-Identifier: MIT
 * Requires integration into the EXISTING authenticated two-node switch, ARQ,
 * original Android Compose UI and unchanged OGM4 AES-GCM implementation.
 */
#ifndef OG_ULTRA_POLICY_H
#define OG_ULTRA_POLICY_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define OG_MAX_RANGE_MODE 3u
#define OG_ULTRA_MODE 4u
#define OG_ULTRA_SF 12u
#define OG_ULTRA_BW_HZ 125000u
#define OG_ULTRA_CR_DENOM 8u /* CR4/8: stronger FEC for intermittent/weak Ultra RF */
#define OG_ULTRA_PREAMBLE 8u
#define OG_ULTRA_LOW_DATA_RATE_OPTIMIZE true
/* For explicit header + CRC + AGC: register 0x1D=0x78 (BW125, 4/8),
 * 0x1E=0xC4 (SF12, CRC), 0x26=0x0C (AGC, LDRO).
 * Do not change the existing driver or transmitter power path blindly. */
#define OG_ULTRA_MODEM_CONFIG1 0x78u
#define OG_ULTRA_MODEM_CONFIG2 0xC4u
#define OG_ULTRA_MODEM_CONFIG3 0x0Cu
#define OG_ULTRA_DOWN_RSSI_DBM (-115)
#define OG_ULTRA_DOWN_SNR_Q4 (-20) /* -5 dB */
#define OG_ULTRA_UP_RSSI_DBM (-103)
#define OG_ULTRA_UP_SNR_Q4 (12) /* +3 dB */
#define OG_ULTRA_MAX_RADIO_BYTES 112u
#define OG_ULTRA_FRAG_WEAK_RADIO_BYTES 72u
#define OG_ULTRA_FRAG_SEVERE_RADIO_BYTES 56u
#define OG_ULTRA_ARQ_HMAC_HDR_BYTES 38u /* illustrative OGM2 22 + 16 tag; verify latest */

typedef enum {
    OG_CONTENT_TEXT = 1,
    OG_CONTENT_DELIVERED_RECEIPT = 2,
    OG_CONTENT_READ_RECEIPT = 3,
    OG_CONTENT_VOICE = 4,
    OG_CONTENT_PHOTO = 5,
    OG_CONTENT_TEXT_FRAG = 6,
    OG_CONTENT_AUTH_CONTROL = 7,
    OG_CONTENT_MEDIA_PART = 8,
    OG_CONTENT_PROFILE_CONTROL = 9
} og_content_kind_t;

typedef struct {
    bool authenticated;
    bool fresh;
    int16_t rssi_dbm;
    int16_t snr_q4;
    uint8_t losses_last_8; /* 0..8, authenticated peer report */
} og_link_quality_t;

typedef struct {
    uint8_t down_streak; /* consecutive observations */
    uint8_t up_streak;
    uint32_t last_change_ms;
} og_ultra_decider_t;

/* Always allow authenticated profile/discovery, delivery/read receipts and text;
 * never allow an outbound photo/voice or a media ciphertext fragment in Ultra.
 * Call BOTH before enqueue AND at the RF worker; the sender UI check alone
 * does not prevent queued media from transmitting after an Auto switch. */
bool og_ultra_allows_content(uint8_t profile, og_content_kind_t kind);

/* Use the exact Semtech airtime formula. 0 means invalid parameters. */
uint32_t og_ultra_airtime_ms(uint8_t sf, uint32_t bw_hz, uint8_t cr_denominator,
                            uint16_t preamble, uint16_t physical_bytes);

/* This is a post-TxDone ACK timeout, NOT total message deadline. */
uint32_t og_ultra_ack_wait_ms(uint16_t ack_air_bytes, uint8_t attempt);

/* RTO budget for entire application fragment; includes TX + post-TxDone ACK.
 * Receiver processing and legal channel duty policies must be enforced above. */
uint32_t og_ultra_fragment_deadline_ms(uint16_t payload_air_bytes,
                                      uint16_t ack_air_bytes,uint8_t attempt);

/* 56..112 bytes TOTAL authenticated OGM2 RF frame; size AFTER authenticated
 * wire header and tag. Low SNR/loss => shorter packets, not a larger window. */
uint16_t og_ultra_max_air_frame(const og_link_quality_t *local,
                                const og_link_quality_t *remote);
uint16_t og_ultra_fragment_data_limit(const og_link_quality_t *local,
                                      const og_link_quality_t *remote,
                                      uint16_t overhead);
/* Ack several radio fragments with ONE authenticated selective bitmap when
 * quality supports it; on a severe link ACK each fragment to bound loss. */
uint8_t og_ultra_ack_window(const og_link_quality_t *local,
                            const og_link_quality_t *remote);
uint16_t og_ultra_fragment_count(uint16_t encrypted_bytes, uint16_t data_limit);
/* Planning estimate, not a guarantee or a true measured throughput. */
uint32_t og_ultra_estimated_rf_ms(uint16_t encrypted_bytes, uint16_t overhead,
                                uint16_t ack_air_bytes,
                                const og_link_quality_t *local,
                                const og_link_quality_t *remote);

/* Qualify Auto transition BEFORE complete packet loss: two authenticated,
 * fresh observations per end, smoothed in the v3.0.3 radio worker. Caller
 * must still negotiate OGM2 OFFER/AGREE and rollback on failure.
 * Return true to request Ultra when currently in existing SF9 MAX mode. */
bool og_ultra_should_request(uint8_t current_mode,
                             const og_link_quality_t *local,
                             const og_link_quality_t *remote,
                             og_ultra_decider_t *state);

/* Recovery is intentionally slow to avoid bouncing between SF12 and SF9.
 * Requires strong bidirectional metrics and a 120s cooldown after a switch. */
bool og_ultra_should_recover(uint8_t current_mode,
                             const og_link_quality_t *local,
                             const og_link_quality_t *remote,
                             og_ultra_decider_t *state, uint32_t now_ms);
#endif
