/* SPDX-License-Identifier: GPL-3.0-or-later
 * One-browser manually triggered two-node profile coordination.
 * This is a protocol change to the authenticated OGM2 profile control frames:
 * the operator's inspection of BOTH boards is authenticated in the offer.
 * The radio HMAC verification happens BEFORE this pure parser is called.
 * This flag is a declaration, NOT a hardware attestation.
 */
#ifndef OFFGRID_LORA_PROFILE_CONTROL_H
#define OFFGRID_LORA_PROFILE_CONTROL_H
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "lora_wire.h"
#include "lora_modes.h"
#define OG_PROFILE_PAYLOAD_BYTES 6u
#define OG_PROFILE_CONFIRM_BOTH_PA 0xA5u
#define OG_PROFILE_AUTO_FLAG 0x40u
#define OG_PROFILE_REJECT_BUSY 0x01u
static inline bool og_profile_mode_valid(uint8_t mode) {
    return mode==OG_MODE_LOW || mode==OG_MODE_BALANCED || mode==OG_MODE_MAX;
}
static inline bool og_profile_high_pa(uint8_t mode) {
    return mode==OG_MODE_LOW || mode==OG_MODE_MAX;
}
static inline bool og_profile_control_valid(const og_frame_t *f) {
    if(!f || !og_profile_mode_valid(f->data[0]) ||
       f->length!=OG_PROFILE_PAYLOAD_BYTES || !f->id)return false;
    uint32_t embedded=0;
    for(unsigned i=0;i<4;i++)embedded|=(uint32_t)f->data[i+1]<<(i*8u);
    if(f->id!=embedded)return false;
    if(f->type==OG_TYPE_PROFILE_REJECT)return f->data[5]==OG_PROFILE_REJECT_BUSY;
    if(f->type!=OG_TYPE_PROFILE_OFFER && f->type!=OG_TYPE_PROFILE_AGREE)return false;
    const uint8_t expected=og_profile_high_pa(f->data[0])?OG_PROFILE_CONFIRM_BOTH_PA:0u;
    return f->data[5]==expected || f->data[5]==(uint8_t)(expected|OG_PROFILE_AUTO_FLAG);
}
static inline og_frame_t og_profile_control_frame(uint8_t type,uint8_t mode,
       const uint8_t sender[6],uint32_t session,uint32_t nonce) {
    og_frame_t f={0};f.magic=OG_MAGIC;f.version=OG_VERSION;f.type=type;
    if(sender)memcpy(f.sender,sender,6);
    f.session=session;f.id=nonce;f.length=OG_PROFILE_PAYLOAD_BYTES;
    f.data[0]=mode;
    for(unsigned i=0;i<4;i++)f.data[i+1]=(uint8_t)(nonce>>(i*8u));
    f.data[5]=og_profile_high_pa(mode)?OG_PROFILE_CONFIRM_BOTH_PA:0u;
    if(type==OG_TYPE_PROFILE_REJECT)f.data[5]=OG_PROFILE_REJECT_BUSY;
    return f;
}
static inline bool og_profile_is_auto(const og_frame_t *f) {
    return f && (f->type==OG_TYPE_PROFILE_OFFER || f->type==OG_TYPE_PROFILE_AGREE) &&
           (f->data[5]&OG_PROFILE_AUTO_FLAG)!=0;
}
#endif
