/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "og_sr_usb.h"
#include <string.h>

uint32_t og_sr_usb_crc32(uint32_t previous,const uint8_t *bytes,size_t n){
    /* Standard reflected CRC32 (poly 0xEDB88320). The API tracks finalized
     * values, i.e. begin with 0 and pass result to next chunk. */
    if(!bytes && n)return previous;
    uint32_t crc=~previous;
    for(size_t j=0;j<n;j++){
        crc^=bytes[j];
        for(unsigned b=0;b<8;b++)
            crc=(crc>>1)^((crc&1u)?UINT32_C(0xEDB88320):0u);
    }
    return ~crc;
}

bool og_sr_usb_begin(og_sr_usb_t *r,uint32_t transfer,size_t expected,
                     uint8_t *buffer,size_t capacity){
    if(!r || r->active || !transfer || !buffer || !expected ||
       expected>capacity)return false;
    memset(r,0,sizeof(*r));r->dest=buffer;r->capacity=capacity;
    r->transfer=transfer;r->expected=expected;r->active=true;return true;
}

bool og_sr_usb_append(og_sr_usb_t *r,uint32_t transfer,uint16_t seq,
                      const uint8_t *bytes,size_t len){
    if(!r || !r->active || transfer!=r->transfer || seq!=r->next_seq ||
       !bytes || !len || len>OG_SR_USB_CHUNK ||
       len>r->expected-r->received)return false;
    memcpy(r->dest+r->received,bytes,len);
    r->crc=og_sr_usb_crc32(r->crc,bytes,len);
    r->received+=len;r->next_seq++;
    return true;
}

bool og_sr_usb_finish(og_sr_usb_t *r,uint32_t transfer,uint32_t expected_crc){
    if(!r || !r->active || r->transfer!=transfer ||
       r->received!=r->expected || r->crc!=expected_crc)return false;
    r->active=false;
    return true;
}

void og_sr_usb_cancel(og_sr_usb_t *r){
    if(!r)return;
    r->active=false;
    /* Deliberately don't wipe caller-owned buffer: application must destroy
     * stale ciphertext and retain no secrets if canceling the transfer. */
}
