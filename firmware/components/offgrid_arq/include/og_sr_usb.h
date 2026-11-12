/* SPDX-License-Identifier: GPL-3.0-or-later
 * Bounded USB-side ciphertext bulk accumulator. It enables the browser to
 * hand one encrypted object to the ESP32 rather than waiting for a Web Serial
 * round-trip after every RF fragment. This is not a radio protocol parser.
 */
#ifndef OG_SR_USB_H
#define OG_SR_USB_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define OG_SR_USB_CHUNK 1024u

typedef struct {
    uint8_t *dest;
    size_t capacity, expected, received;
    uint32_t transfer, crc;
    uint16_t next_seq;
    bool active;
} og_sr_usb_t;

/* CRC32 checks USB transport integrity; AES-GCM separately authenticates the
 * ciphertext and application payload. CRC32 is NOT a security primitive. */
uint32_t og_sr_usb_crc32(uint32_t previous,const uint8_t *bytes,size_t n);
bool og_sr_usb_begin(og_sr_usb_t *rx,uint32_t transfer,size_t expected,
                     uint8_t *buffer,size_t buffer_capacity);
bool og_sr_usb_append(og_sr_usb_t *rx,uint32_t transfer,uint16_t seq,
                      const uint8_t *ciphertext,size_t len);
bool og_sr_usb_finish(og_sr_usb_t *rx,uint32_t transfer,uint32_t expected_crc);
void og_sr_usb_cancel(og_sr_usb_t *rx);
#endif
