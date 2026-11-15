/* SPDX-License-Identifier: GPL-3.0-or-later
 * OffGrid selective-repeat LoRa transport, hardware-independent prototype.
 * Application payload MUST be AES-256-GCM ciphertext before entering this
 * layer. Every on-air frame MUST pass HMAC-SHA256-128 signing/verification.
 */
#ifndef OG_SR_H
#define OG_SR_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Radio FIFO permits 255 bytes. Bounded at 254 bytes including HMAC (one byte below SX1278 maximum). */
#define OG_SR_VERSION 1u
#define OG_SR_HEADER 14u
#define OG_SR_TAG 16u
#define OG_SR_PAYLOAD 224u
#define OG_SR_WIRE_MAX (OG_SR_HEADER + OG_SR_PAYLOAD + OG_SR_TAG)
#define OG_SR_ACK_BYTES 4u
#define OG_SR_MAX_FRAGMENTS 32u
#define OG_SR_MAX_TRANSFER (OG_SR_MAX_FRAGMENTS * OG_SR_PAYLOAD)
#define OG_SR_MAX_WINDOW 6u
#define OG_SR_MAX_ATTEMPTS 5u
#define OG_SR_ULTRA_MAX_ATTEMPTS 12u /* persistent sender requeues after failure */
#define OG_SR_FLAG_END_BURST 1u
#define OG_SR_FLAG_ULTRA 2u
#define OG_SR_FLAG_SIZE_MASK 12u
/* MAX/SF9 text fragments: 128, 160, 192 bytes. Media stays 224. */
#define OG_SR_FLAG_RANGE 16u
#define OG_SR_ULTRA_MIN_PAYLOAD 48u
#define OG_SR_ULTRA_MAX_PAYLOAD 96u
static inline uint8_t og_sr_payload_from_flags(uint8_t flags) {
    return (flags & OG_SR_FLAG_ULTRA) ? (uint8_t)(48u+16u*((flags>>2u)&3u)) :
           (flags & OG_SR_FLAG_RANGE) ? (uint8_t)(128u+32u*((flags>>2u)&3u)) : OG_SR_PAYLOAD;
}
static inline uint8_t og_sr_flags_from_payload(uint8_t n) {
    return n==OG_SR_PAYLOAD ? 0u : (n>=48u && n<=96u && (n-48u)%16u==0u) ?
           (uint8_t)(OG_SR_FLAG_ULTRA | ((n-48u)/16u<<2u)) :
           (n>=128u && n<=192u && (n-128u)%32u==0u) ?
           (uint8_t)(OG_SR_FLAG_RANGE | ((n-128u)/32u<<2u)) : 0xFFu;
}

typedef enum {
    OG_SR_DATA=1,
    OG_SR_BITMAP_ACK=2
} og_sr_type_t;

typedef enum {
    OG_SR_MORE=0,
    OG_SR_WAIT_ACK=1,
    OG_SR_COMPLETE=2,
    OG_SR_FAILED=3
} og_sr_phase_t;

typedef struct {
    og_sr_type_t type;
    uint32_t session;
    uint32_t transfer;
    uint8_t index;
    uint8_t count;
    uint8_t flags;
    uint8_t length;
    uint8_t payload[OG_SR_PAYLOAD];
    uint32_t bitmap; /* Valid only for BITMAP_ACK. */
} og_sr_frame_t;

typedef bool (*og_sr_mac_sign_fn)(void *ctx,const uint8_t *data,size_t n,
                                  uint8_t out16[OG_SR_TAG]);
typedef bool (*og_sr_mac_verify_fn)(void *ctx,const uint8_t *data,size_t n,
                                    const uint8_t tag16[OG_SR_TAG]);

/* MAC is mandatory, including for ACK frames. Never pass NULL callbacks. */
bool og_sr_encode(const og_sr_frame_t *frame, uint8_t *out, size_t capacity,
                  size_t *written, og_sr_mac_sign_fn sign, void *crypto);
bool og_sr_decode(const uint8_t *wire,size_t n,og_sr_frame_t *frame,
                  og_sr_mac_verify_fn verify,void *crypto);
/* Max/Ultra-only 16-byte ACK: the full original HMAC-SHA256-128 over the
 * exact 18-byte ACK preimage, not an 8-byte/unauthenticated shortcut.
 * A <=3-fragment send window allows the sender to authenticate at most
 * seven outstanding bitmap possibilities. Requires both peers updated. */
bool og_sr_compact_ack_encode(const og_sr_frame_t *ack,uint8_t tag16[OG_SR_TAG],
                              og_sr_mac_sign_fn sign,void *crypto);

typedef struct {
    uint32_t session, transfer;
    const uint8_t *ciphertext; /* Owned by caller; live until completion. */
    size_t byte_count;
    uint8_t fragments, window, fragment_payload;
    uint8_t attempts[OG_SR_MAX_FRAGMENTS];
    uint32_t acknowledged, transmitted, current_burst;
    uint32_t deadline_ms;
    og_sr_phase_t phase;
} og_sr_tx_t;

/* Client must supply freshly unpredictable IDs and an AES-GCM ciphertext.
 * This layer deliberately does NOT generate session IDs or encrypt plaintext. */
bool og_sr_tx_init_ex(og_sr_tx_t *tx,uint32_t session,uint32_t transfer,
                   const uint8_t *ciphertext,size_t n,uint8_t window,uint8_t fragment_payload);
bool og_sr_tx_init(og_sr_tx_t *tx,uint32_t session,uint32_t transfer,
                   const uint8_t *ciphertext,size_t n,uint8_t window);
/* Pick one packet for the radio owner. Caller performs exactly one TX and
 * then records success/failure with og_sr_tx_done. */
bool og_sr_tx_pick(og_sr_tx_t *tx,og_sr_frame_t *frame);
void og_sr_tx_done(og_sr_tx_t *tx,const og_sr_frame_t *frame,bool tx_done,
                   uint32_t at_ms,uint32_t ack_timeout_ms);
/* Only call after a MAC-verified ACK from the expected peer. */
bool og_sr_tx_ack(og_sr_tx_t *tx,const og_sr_frame_t *ack);
bool og_sr_compact_ack_decode(const uint8_t tag16[OG_SR_TAG],
                              const og_sr_tx_t *tx,og_sr_frame_t *ack,
                              og_sr_mac_verify_fn verify,void *crypto);
/* Number of the highest retry attempt in flight, for profile-aware ACK RTO. */
uint8_t og_sr_tx_largest_attempt(const og_sr_tx_t *tx);
/* Expired ACK time allows retransmission of *unacknowledged* fragments only.
 * Caller owns the airtime, contention policy, and timebase. */
void og_sr_tx_tick(og_sr_tx_t *tx,uint32_t now_ms);

typedef struct {
    uint32_t session,transfer;
    uint8_t expected, fragment_payload;
    uint32_t received;
    uint8_t *buffer;
    size_t capacity, received_bytes;
    uint8_t last_length;
    uint32_t ack_due_ms;
    bool ack_pending;
} og_sr_rx_t;

/* External bounded buffer holds ciphertext only, never decrypted plaintext. */
bool og_sr_rx_init_ex(og_sr_rx_t *rx,uint32_t session,uint32_t transfer,
                   uint8_t count,uint8_t *ciphertext_buffer,size_t capacity,uint8_t fragment_payload);
bool og_sr_rx_init(og_sr_rx_t *rx,uint32_t session,uint32_t transfer,
                   uint8_t count,uint8_t *ciphertext_buffer,size_t capacity);
/* Call ONLY after og_sr_decode HMAC-verifies frame. */
bool og_sr_rx_push(og_sr_rx_t *rx,const og_sr_frame_t *data,uint32_t now_ms,
                   uint32_t delayed_ack_ms);
bool og_sr_rx_complete(const og_sr_rx_t *rx);
size_t og_sr_rx_size(const og_sr_rx_t *rx);
/* ACK bitmaps are generated after end-of-burst or a receive silence timer. */
bool og_sr_rx_ack_due(const og_sr_rx_t *rx,uint32_t now_ms);
bool og_sr_rx_make_ack(og_sr_rx_t *rx,og_sr_frame_t *ack);

#ifdef __cplusplus
}
#endif
#endif
