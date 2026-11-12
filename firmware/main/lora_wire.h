/*
 * OffGrid Messenger LoRa v1.1: OGM2-compatible, authenticated SX1278 framing.
 * This header has NO ESP-IDF dependencies, so it can be checked on a host.
 * The independent browser OGM4 envelope provides AES-GCM confidentiality.
 */
#ifndef OFFGRID_LORA_WIRE_H
#define OFFGRID_LORA_WIRE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define OG_MAGIC UINT32_C(0x4F474D32)
#define OG_VERSION 2u
#define OG_MAX_DATA 180u
#define OG_AUTH_BYTES 16u
#define OG_FRAME_HDR_BYTES 22u
#define OG_FRAME_MAX (OG_FRAME_HDR_BYTES + OG_MAX_DATA)
#define OG_AIR_MAX (OG_FRAME_MAX + OG_AUTH_BYTES)
#define OG_TYPE_DATA 1u
#define OG_TYPE_DELIVERED 2u
#define OG_TYPE_READ 3u
#define OG_TYPE_HELLO 4u
#define OG_TYPE_RADIO_ACK 8u
#define OG_TYPE_PROFILE_OFFER 9u
#define OG_TYPE_PROFILE_AGREE 10u

/* Packed representation intentionally preserves the original OGM2 packet header.
 * Both ESP32-C3 devices use the same little-endian frame layout. */
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint8_t sender[6];
    uint32_t session;
    uint32_t id;
    uint16_t length;
    uint8_t data[OG_MAX_DATA];
} og_frame_t;

static inline bool og_wire_valid(const og_frame_t *f, size_t n) {
    if (!f || n < OG_FRAME_HDR_BYTES || n > OG_FRAME_MAX ||
        f->magic != OG_MAGIC || f->version != OG_VERSION ||
        f->length > OG_MAX_DATA || n != OG_FRAME_HDR_BYTES + f->length)
        return false;
    if (f->type != OG_TYPE_DATA && f->type != OG_TYPE_DELIVERED &&
        f->type != OG_TYPE_READ && f->type != OG_TYPE_HELLO &&
        f->type != OG_TYPE_RADIO_ACK &&
        f->type != OG_TYPE_PROFILE_OFFER && f->type != OG_TYPE_PROFILE_AGREE) return false;
    if(f->type==OG_TYPE_PROFILE_OFFER || f->type==OG_TYPE_PROFILE_AGREE)
        return f->length==5 && f->data[0]>=1 && f->data[0]<=3;
    return f->type == OG_TYPE_DATA ? f->length > 0 : f->length == 0;
}
static inline size_t og_wire_length(const og_frame_t *f) {
    return OG_FRAME_HDR_BYTES + (f ? f->length : 0);
}
/* SF7/BW125 fixed v1.1 profile. Small HMAC ACK frame ~82 ms airtime.
 * Budget turnaround + receiver processing + RF airtime + scheduling margin,
 * not a multi-second generic ACK timeout. Time starts AFTER TxDone.
 * Lost packets get bounded backoff. */
static inline uint32_t og_ack_wait_ms(unsigned attempt) {
    if (attempt > 5) attempt = 5;
    return 460u + attempt * 180u;
}
#endif
