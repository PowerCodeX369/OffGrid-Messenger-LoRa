/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "og_sr.h"
#include <string.h>

static void le32_put(uint8_t *b,uint32_t v) {
    for(unsigned i=0;i<4;i++)b[i]=(uint8_t)(v>>(i*8));
}
static uint32_t le32_get(const uint8_t *b) {
    uint32_t r=0;
    for(unsigned i=0;i<4;i++)r|=(uint32_t)b[i]<<(i*8);
    return r;
}
static uint32_t expected_mask(uint8_t n) {
    return n==32u?UINT32_MAX:((1u<<n)-1u);
}
static bool frame_ok(const og_sr_frame_t *f) {
    if(!f || !f->session || !f->transfer || f->count<1 ||
       f->count>OG_SR_MAX_FRAGMENTS || (f->flags&~OG_SR_FLAG_END_BURST))return false;
    if(f->type==OG_SR_DATA){
        if(f->index>=f->count || f->length<1 || f->length>OG_SR_PAYLOAD)return false;
        if(f->index+1<f->count && f->length!=OG_SR_PAYLOAD)return false;
        return true;
    }
    if(f->type==OG_SR_BITMAP_ACK){
        return f->index==0 && f->length==OG_SR_ACK_BYTES && f->flags==0 &&
               (f->bitmap&~expected_mask(f->count))==0;
    }
    return false;
}

bool og_sr_encode(const og_sr_frame_t *f,uint8_t *out,size_t capacity,
                  size_t *written,og_sr_mac_sign_fn sign,void *crypto) {
    if(!out || !written || !sign || !frame_ok(f))return false;
    size_t content=f->type==OG_SR_DATA?f->length:OG_SR_ACK_BYTES;
    const size_t total=OG_SR_HEADER+content+OG_SR_TAG;
    if(total>OG_SR_WIRE_MAX || capacity<total)return false;
    out[0]=OG_SR_VERSION;
    out[1]=(uint8_t)f->type;
    out[2]=f->flags;
    out[3]=f->index;
    out[4]=f->count;
    out[5]=(uint8_t)content;
    le32_put(out+6,f->session);le32_put(out+10,f->transfer);
    if(f->type==OG_SR_DATA)memcpy(out+OG_SR_HEADER,f->payload,content);
    else le32_put(out+OG_SR_HEADER,f->bitmap);
    if(!sign(crypto,out,OG_SR_HEADER+content,out+OG_SR_HEADER+content))return false;
    *written=total;
    return true;
}

bool og_sr_decode(const uint8_t *wire,size_t n,og_sr_frame_t *f,
                  og_sr_mac_verify_fn verify,void *crypto) {
    if(!wire || !f || !verify || n<OG_SR_HEADER+OG_SR_TAG+1 ||
       n>OG_SR_WIRE_MAX || wire[0]!=OG_SR_VERSION)return false;
    const size_t bytes=n-OG_SR_HEADER-OG_SR_TAG;
    if(wire[5]!=bytes || bytes>OG_SR_PAYLOAD)return false;
    if(!verify(crypto,wire,OG_SR_HEADER+bytes,wire+OG_SR_HEADER+bytes))return false;
    og_sr_frame_t tmp={0};
    tmp.type=(og_sr_type_t)wire[1];tmp.flags=wire[2];
    tmp.index=wire[3];tmp.count=wire[4];tmp.length=wire[5];
    tmp.session=le32_get(wire+6);tmp.transfer=le32_get(wire+10);
    if(tmp.type==OG_SR_DATA)memcpy(tmp.payload,wire+OG_SR_HEADER,bytes);
    else if(tmp.type==OG_SR_BITMAP_ACK && bytes==OG_SR_ACK_BYTES)
        tmp.bitmap=le32_get(wire+OG_SR_HEADER);
    else return false;
    if(!frame_ok(&tmp))return false;
    *f=tmp;
    return true;
}

bool og_sr_tx_init(og_sr_tx_t *t,uint32_t session,uint32_t transfer,
                   const uint8_t *ciphertext,size_t n,uint8_t window) {
    if(!t || !session || !transfer || !ciphertext || n==0 ||
       n>OG_SR_MAX_TRANSFER || window==0 || window>OG_SR_MAX_WINDOW)return false;
    memset(t,0,sizeof(*t));
    t->session=session;t->transfer=transfer;
    t->ciphertext=ciphertext;t->byte_count=n;t->window=window;
    t->fragments=(uint8_t)((n+OG_SR_PAYLOAD-1u)/OG_SR_PAYLOAD);
    t->phase=OG_SR_MORE;
    return true;
}

bool og_sr_tx_pick(og_sr_tx_t *t,og_sr_frame_t *f) {
    if(!t || !f || t->phase!=OG_SR_MORE)return false;
    const uint32_t needed=expected_mask(t->fragments);
    const uint32_t pending=needed & ~t->acknowledged;
    if(!pending){t->phase=OG_SR_COMPLETE;return false;}
    uint8_t base=0;
    while(base<t->fragments && !(pending&(1u<<base)))base++;
    const uint8_t exclusive=(uint8_t)((base+t->window<t->fragments)?base+t->window:t->fragments);
    uint8_t which=base;
    while(which<exclusive && (!(pending&(1u<<which)) || (t->current_burst&(1u<<which))))which++;
    if(which==exclusive){t->phase=OG_SR_WAIT_ACK;return false;}
    if(t->attempts[which]>=OG_SR_MAX_ATTEMPTS){t->phase=OG_SR_FAILED;return false;}
    memset(f,0,sizeof(*f));
    f->type=OG_SR_DATA;f->session=t->session;f->transfer=t->transfer;
    f->index=which;f->count=t->fragments;
    const size_t offset=(size_t)which*OG_SR_PAYLOAD;
    const size_t remaining=t->byte_count-offset;
    f->length=(uint8_t)(remaining>=OG_SR_PAYLOAD?OG_SR_PAYLOAD:remaining);
    memcpy(f->payload,t->ciphertext+offset,f->length);
    /* Only mark END on the last new packet of this window. */
    bool any_following=false;
    for(uint8_t j=(uint8_t)(which+1);j<exclusive;j++) {
        if((pending&(1u<<j)) && !(t->current_burst&(1u<<j)))any_following=true;
    }
    if(!any_following)f->flags=OG_SR_FLAG_END_BURST;
    return true;
}

void og_sr_tx_done(og_sr_tx_t *t,const og_sr_frame_t *f,bool success,
                   uint32_t at_ms,uint32_t ack_timeout_ms) {
    if(!t || !f || t->phase!=OG_SR_MORE || f->type!=OG_SR_DATA ||
       f->session!=t->session || f->transfer!=t->transfer || f->count!=t->fragments ||
       f->index>=t->fragments || !ack_timeout_ms)return;
    if(!success){
        /* Not transmitted on air: retry later without treating as a peer loss. */
        return;
    }
    const uint32_t bit=1u<<f->index;
    if(t->current_burst&bit)return;
    t->attempts[f->index]++;
    t->current_burst|=bit;t->transmitted|=bit;
    if(f->flags&OG_SR_FLAG_END_BURST){
        t->phase=OG_SR_WAIT_ACK;
        t->deadline_ms=at_ms+ack_timeout_ms;
    }
}

bool og_sr_tx_ack(og_sr_tx_t *t,const og_sr_frame_t *ack) {
    if(!t || !ack || (t->phase!=OG_SR_WAIT_ACK && t->phase!=OG_SR_MORE) ||
       ack->type!=OG_SR_BITMAP_ACK || ack->session!=t->session ||
       ack->transfer!=t->transfer || ack->count!=t->fragments ||
       !frame_ok(ack) || (ack->bitmap&~t->transmitted))return false;
    t->acknowledged|=ack->bitmap;
    t->current_burst=0;
    if(t->acknowledged==expected_mask(t->fragments))t->phase=OG_SR_COMPLETE;
    else t->phase=OG_SR_MORE;
    return true;
}

void og_sr_tx_tick(og_sr_tx_t *t,uint32_t now_ms) {
    if(!t || t->phase!=OG_SR_WAIT_ACK)return;
    /* Wrap-safe comparison (timeouts must be < 2^31 ms). */
    if((int32_t)(now_ms-t->deadline_ms)>=0){
        t->current_burst=0;
        t->phase=OG_SR_MORE;
    }
}

bool og_sr_rx_init(og_sr_rx_t *r,uint32_t session,uint32_t transfer,
                   uint8_t count,uint8_t *buffer,size_t capacity) {
    if(!r || !session || !transfer || !buffer || count==0 ||
       count>OG_SR_MAX_FRAGMENTS || capacity<(size_t)count*OG_SR_PAYLOAD)return false;
    memset(r,0,sizeof(*r));
    r->session=session;r->transfer=transfer;r->expected=count;
    r->buffer=buffer;r->capacity=capacity;
    return true;
}

bool og_sr_rx_push(og_sr_rx_t *r,const og_sr_frame_t *f,uint32_t now_ms,
                   uint32_t delayed_ack_ms) {
    if(!r || !f || !frame_ok(f) || f->type!=OG_SR_DATA ||
       f->session!=r->session || f->transfer!=r->transfer ||
       f->count!=r->expected || delayed_ack_ms==0)return false;
    const uint32_t bit=1u<<f->index;
    if(!(r->received&bit)){
        const size_t offset=(size_t)f->index*OG_SR_PAYLOAD;
        if(offset+f->length>r->capacity)return false;
        memcpy(r->buffer+offset,f->payload,f->length);
        r->received|=bit;r->received_bytes+=f->length;
        if(f->index+1==r->expected)r->last_length=f->length;
    }
    r->ack_pending=true;
    /* Leave an explicit RX->TX turnaround even for END_BURST. The caller
     * chooses delayed_ack_ms using the active PHY's packet airtime. */
    uint32_t end_delay=delayed_ack_ms/3u;
    if(end_delay<25u)end_delay=25u;
    if(end_delay>80u)end_delay=80u;
    r->ack_due_ms=now_ms+((f->flags&OG_SR_FLAG_END_BURST)?end_delay:delayed_ack_ms);
    return true;
}

bool og_sr_rx_complete(const og_sr_rx_t *r) {
    return r && r->received==expected_mask(r->expected) && r->last_length>0;
}

size_t og_sr_rx_size(const og_sr_rx_t *r) {
    return og_sr_rx_complete(r)?((size_t)(r->expected-1u)*OG_SR_PAYLOAD+r->last_length):0;
}

bool og_sr_rx_ack_due(const og_sr_rx_t *r,uint32_t now_ms) {
    return r && r->ack_pending && (int32_t)(now_ms-r->ack_due_ms)>=0;
}

bool og_sr_rx_make_ack(og_sr_rx_t *r,og_sr_frame_t *ack) {
    if(!r || !ack || !r->ack_pending)return false;
    memset(ack,0,sizeof(*ack));
    ack->type=OG_SR_BITMAP_ACK;
    ack->session=r->session;ack->transfer=r->transfer;ack->count=r->expected;
    ack->length=OG_SR_ACK_BYTES;ack->bitmap=r->received;
    r->ack_pending=false;
    return true;
}
