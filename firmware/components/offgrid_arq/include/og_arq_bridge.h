/* OffGrid v1.6 radio-owner integration for the independently tested SR core.
 * No radio, crypto or serial APIs in this file: callback boundaries are pure C.
 * All functions execute ONLY in the one radio-worker task.
 */
#ifndef OG_ARQ_BRIDGE_H
#define OG_ARQ_BRIDGE_H
#include "og_sr.h"
#include "og_sr_usb.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define OG_BRIDGE_STAGE_CHUNK 160u
#define OG_BRIDGE_ACTIVE_TIMEOUT_MS 30000u

typedef bool (*og_bridge_radio_fn)(void *, const uint8_t *, size_t);
typedef void (*og_bridge_line_fn)(void *, const char *);
typedef uint32_t (*og_bridge_millis_fn)(void *);
typedef struct {
    void *context;
    og_bridge_radio_fn radio_tx;
    og_bridge_line_fn serial_line;
    og_bridge_millis_fn now_ms;
    og_sr_mac_sign_fn sign;
    og_sr_mac_verify_fn verify;
    void *crypto;
} og_arq_io_t;
typedef struct {
    og_sr_usb_t usb;
    uint32_t usb_session;
    uint8_t usb_bytes[OG_SR_MAX_TRANSFER];
    uint8_t tx_bytes[OG_SR_MAX_TRANSFER];
    og_sr_tx_t tx;
    bool tx_used, tx_urgent, tx_sent_announced;
    uint32_t tx_last_at, next_tx_at, last_progress;
    uint8_t tx_local_errors;
    uint8_t rx_bytes[OG_SR_MAX_TRANSFER];
    og_sr_rx_t rx;
    bool rx_used, rx_forwarding, rx_forwarded;
    uint16_t rx_forward_offset, rx_seq;
    uint32_t rx_last_at;
    uint32_t last_session, last_transfer, last_bitmap;
    uint8_t last_count;
    bool last_valid, duplicate_ack_pending;
    uint32_t duplicate_ack_due;
    uint32_t tx_frames, retransmits, bitmap_acks_tx, bitmap_acks_rx;
    uint32_t ack_timeouts, rx_duplicates, reject_auth, usb_errors;
} og_arq_bridge_t;

void og_arq_bridge_init(og_arq_bridge_t *s);
bool og_arq_bridge_busy(const og_arq_bridge_t *s);
void og_arq_bridge_cancel(og_arq_bridge_t *s,const og_arq_io_t *io,const char *reason);
/* Split a browser's 64-bit AES-GCM transferId into two LE32 words. */
bool og_arq_bridge_begin(og_arq_bridge_t *s,uint32_t session,uint32_t transfer,
                          size_t n,bool urgent,const og_arq_io_t *io);
bool og_arq_bridge_chunk(og_arq_bridge_t *s,uint32_t session,uint32_t transfer,
                          uint16_t seq,const uint8_t *data,size_t n,const og_arq_io_t *io);
bool og_arq_bridge_end(og_arq_bridge_t *s,uint32_t session,uint32_t transfer,
                        uint32_t crc,const og_arq_io_t *io);
/* Returns true for an ARQ-format radio frame even if authentication failed.
 * This protects the legacy OGM2 parser from accepting ARQ bytes as controls. */
bool og_arq_bridge_receive(og_arq_bridge_t *s,const uint8_t *wire,size_t n,
                            uint32_t now,unsigned ack_silence_ms,const og_arq_io_t *io);
/* Run once per radio-loop turn; returns true if radio TX or USB emission used
 * the current opportunity (call again after RX polling). */
bool og_arq_bridge_service(og_arq_bridge_t *s,uint32_t now,uint32_t entropy,
                            unsigned ack_silence_ms,unsigned ack_timeout_ms,
                            bool allow_data_tx,const char *peer_mac,
                            const og_arq_io_t *io);
#endif
