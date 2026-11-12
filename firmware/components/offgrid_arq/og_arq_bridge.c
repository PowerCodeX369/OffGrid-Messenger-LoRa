/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "og_arq_bridge.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static void transfer_id(uint32_t session,uint32_t transfer,char out[17]) {
    uint8_t b[8];
    for(unsigned i=0;i<4;i++){
        b[i]=(uint8_t)(session>>(8*i));
        b[i+4]=(uint8_t)(transfer>>(8*i));
    }
    const char *hex="0123456789ABCDEF";
    for(unsigned i=0;i<8;i++){out[i*2]=hex[b[i]>>4];out[i*2+1]=hex[b[i]&15];}
    out[16]=0;
}
static void emit(const og_arq_io_t *io,const char *line){
    if(io && io->serial_line)io->serial_line(io->context,line);
}
static void emit_failure(const og_arq_io_t *io,uint32_t session,uint32_t transfer,const char *reason){
    char id[17],line[112];transfer_id(session,transfer,id);
    snprintf(line,sizeof(line),"BULKFAILED|%s|%s",id,reason);emit(io,line);
}
static uint32_t bitmask(uint8_t n){return n==32?UINT32_MAX:(1u<<n)-1u;}
void og_arq_bridge_init(og_arq_bridge_t *s){if(s)memset(s,0,sizeof(*s));}
bool og_arq_bridge_busy(const og_arq_bridge_t *s){
    return s && (s->usb.active || s->tx_used || (s->rx_used && !s->rx_forwarded));
}
void og_arq_bridge_cancel(og_arq_bridge_t *s,const og_arq_io_t *io,const char *reason){
    if(!s)return;
    if(s->tx_used)emit_failure(io,s->tx.session,s->tx.transfer,reason?reason:"CANCELED");
    if(s->usb.active)emit_failure(io,s->usb_session,s->usb.transfer,"STAGING_CANCELED");
    og_sr_usb_cancel(&s->usb);s->tx_used=false;s->rx_used=false;
    s->rx_forwarding=false;s->rx_forwarded=false;s->duplicate_ack_pending=false;
}
bool og_arq_bridge_begin(og_arq_bridge_t *s,uint32_t session,uint32_t transfer,
                          size_t n,bool urgent,const og_arq_io_t *io){
    if(!s||s->usb.active||s->tx_used||!session||!transfer||n>OG_SR_MAX_TRANSFER||
       !og_sr_usb_begin(&s->usb,transfer,n,s->usb_bytes,sizeof(s->usb_bytes))){
        if(s)s->usb_errors++;
        emit_failure(io,session,transfer,"BUSY_OR_BAD_SIZE");return false;
    }
    s->usb_session=session;s->tx_urgent=urgent;
    char id[17],line[44];transfer_id(session,transfer,id);
    snprintf(line,sizeof(line),"BULKREADY|%s",id);emit(io,line);
    return true;
}
bool og_arq_bridge_chunk(og_arq_bridge_t *s,uint32_t session,uint32_t transfer,
                          uint16_t seq,const uint8_t *data,size_t n,const og_arq_io_t *io){
    if(!s || session!=s->usb_session ||
       !og_sr_usb_append(&s->usb,transfer,seq,data,n)){
        if(s)s->usb_errors++;
        emit_failure(io,session,transfer,"STAGING_SEQUENCE_OR_LENGTH");
        if(s)og_sr_usb_cancel(&s->usb);
        return false;
    }
    /* A browser writes up to four USB lines in one bounded burst. Emit a
     * stage checkpoint only after the final member of that burst or after
     * the final ciphertext byte; errors always emit BULKFAILED immediately.
     * This avoids seven USB request/response round trips for a 1-KB probe. */
    if(((seq+1u)%OG_BRIDGE_USB_BATCH)==0u || s->usb.received==s->usb.expected){
        char id[17],line[52];transfer_id(session,transfer,id);
        snprintf(line,sizeof(line),"BULKSTEP|%s|%u",id,(unsigned)seq);emit(io,line);
    }
    return true;
}
bool og_arq_bridge_end(og_arq_bridge_t *s,uint32_t session,uint32_t transfer,
                        uint32_t crc,const og_arq_io_t *io){
    if(!s||session!=s->usb_session||!og_sr_usb_finish(&s->usb,transfer,crc)||s->tx_used){
        if(s){s->usb_errors++;og_sr_usb_cancel(&s->usb);}
        emit_failure(io,session,transfer,"STAGING_CRC_OR_LENGTH");return false;
    }
    memcpy(s->tx_bytes,s->usb_bytes,s->usb.received);
    if(!og_sr_tx_init(&s->tx,session,transfer,s->tx_bytes,s->usb.received,OG_SR_MAX_WINDOW)){
        emit_failure(io,session,transfer,"INVALID_TRANSFER");return false;
    }
    s->tx_used=true;s->tx_sent_announced=false;s->last_progress=0;
    s->tx_local_errors=0;s->next_tx_at=0;
    s->transfer_started_ms=io && io->now_ms?io->now_ms(io->context):0;
    s->first_frame_ms=0;
    s->transfer_frames_base=s->tx_frames;
    s->transfer_retries_base=s->retransmits;
    s->transfer_timeouts_base=s->ack_timeouts;
    char id[17],line[50];transfer_id(session,transfer,id);
    snprintf(line,sizeof(line),"BULKQUEUED|%s",id);emit(io,line);
    return true;
}
bool og_arq_bridge_receive(og_arq_bridge_t *s,const uint8_t *wire,size_t n,
                            uint32_t now,unsigned ack_silence_ms,const og_arq_io_t *io){
    if(!s||!wire||!n||wire[0]!=OG_SR_VERSION)return false;
    og_sr_frame_t frame={0};
    if(!io||!io->verify||!og_sr_decode(wire,n,&frame,io->verify,io->crypto)){
        s->reject_auth++;return true;
    }
    if(frame.type==OG_SR_BITMAP_ACK){
        if(s->tx_used && og_sr_tx_ack(&s->tx,&frame)){
            s->bitmap_acks_rx++;
            char id[17],line[85];transfer_id(s->tx.session,s->tx.transfer,id);
            const unsigned count=(unsigned)__builtin_popcount(s->tx.acknowledged);
            if(count>s->last_progress){
                s->last_progress=count;
                snprintf(line,sizeof(line),"BULKPROGRESS|%s|%u|%u",id,count,s->tx.fragments);
                emit(io,line);
            }
        }
        return true;
    }
    if(frame.type!=OG_SR_DATA)return true;
    if(s->last_valid && frame.session==s->last_session && frame.transfer==s->last_transfer){
        s->rx_duplicates++;s->duplicate_ack_pending=true;s->duplicate_ack_due=now+35;
        return true;
    }
    if(s->rx_used && (s->rx.session!=frame.session||s->rx.transfer!=frame.transfer)){
        if(s->rx_forwarding && !s->rx_forwarded)
            return true; /* Preserve ciphertext while the USB client reads it. */
        /* One outstanding encrypted transfer per sender; a NEW authenticated
         * transfer means the peer already moved past an abandoned partial
         * one. Evict incomplete state now instead of blocking its subsequent
         * messages for 30 seconds after a lost RF transfer. */
        /* Save complete transfer bitmap to ACK late duplicates. */
        if(og_sr_rx_complete(&s->rx)){
            s->last_session=s->rx.session;s->last_transfer=s->rx.transfer;
            s->last_bitmap=s->rx.received;s->last_count=s->rx.expected;s->last_valid=true;
        }
        s->rx_used=false;s->rx_forwarding=false;s->rx_forwarded=false;
    }
    if(!s->rx_used){
        if(!og_sr_rx_init(&s->rx,frame.session,frame.transfer,frame.count,
                           s->rx_bytes,sizeof(s->rx_bytes)))return true;
        s->rx_used=true;s->rx_forwarded=false;s->rx_forwarding=false;
    }
    if(s->rx.received & (1u<<frame.index))s->rx_duplicates++;
    /* Caller ensures data was authenticated before this call. */
    if(!og_sr_rx_push(&s->rx,&frame,now,ack_silence_ms)){return true;}
    s->rx_last_at=now;
    return true;
}
static bool radio_send(const og_arq_io_t *io,const og_sr_frame_t *frame){
    uint8_t wire[OG_SR_WIRE_MAX];size_t n=0;
    if(!io||!io->sign||!io->radio_tx||
       !og_sr_encode(frame,wire,sizeof(wire),&n,io->sign,io->crypto))return false;
    return io->radio_tx(io->context,wire,n);
}
static void emit_rx(og_arq_bridge_t *s,const char *peer_mac,const og_arq_io_t *io){
    if(!s->rx_forwarding)return;
    char id[17],line[420];transfer_id(s->rx.session,s->rx.transfer,id);
    const size_t total=og_sr_rx_size(&s->rx);
    if(s->rx_seq==0 && s->rx_forward_offset==0){
        snprintf(line,sizeof(line),"RXBSTART|%s|%lu|%s",id,(unsigned long)total,
                 peer_mac?peer_mac:"00:00:00:00:00:00");
        emit(io,line);
    }
    if(s->rx_forward_offset<total){
        const size_t at=s->rx_forward_offset;
        size_t n=total-at;if(n>OG_BRIDGE_STAGE_CHUNK)n=OG_BRIDGE_STAGE_CHUNK;
        const char *h="0123456789ABCDEF";
        const int k=snprintf(line,sizeof(line),"RXBCHUNK|%s|%u|",id,(unsigned)s->rx_seq);
        if(k<0||(size_t)k+n*2>=sizeof(line))return;
        for(size_t j=0;j<n;j++){
            uint8_t b=s->rx_bytes[at+j];line[k+j*2]=h[b>>4];line[k+j*2+1]=h[b&15];
        }
        line[k+n*2]=0;emit(io,line);
        s->rx_forward_offset+=(uint16_t)n;s->rx_seq++;
    }else{
        const uint32_t crc=og_sr_usb_crc32(0,s->rx_bytes,total);
        snprintf(line,sizeof(line),"RXBDONE|%s|%08" PRIX32,id,crc);emit(io,line);
        s->rx_forwarding=false;s->rx_forwarded=true;
    }
}
bool og_arq_bridge_service(og_arq_bridge_t *s,uint32_t now,uint32_t entropy,
                            unsigned ack_silence_ms,unsigned ack_timeout_ms,
                            bool allow_data_tx,const char *peer_mac,
                            const og_arq_io_t *io){
    if(!s||!io||!io->radio_tx||!io->sign)return false;
    if(s->duplicate_ack_pending && (int32_t)(now-s->duplicate_ack_due)>=0){
        og_sr_frame_t ack={.type=OG_SR_BITMAP_ACK,.session=s->last_session,
            .transfer=s->last_transfer,.count=s->last_count,
            .length=OG_SR_ACK_BYTES,.bitmap=s->last_bitmap};
        if(radio_send(io,&ack)){s->bitmap_acks_tx++;s->duplicate_ack_pending=false;}
        else s->duplicate_ack_due=now+100;
        return true;
    }
    if(s->rx_used && s->rx.ack_pending){
        /* A shorter timeout must never shorten an already scheduled ACK.
         * The caller passes profile-dependent silence computed in real ms. */
        if(og_sr_rx_ack_due(&s->rx,now)){
            og_sr_frame_t ack={0};
            if(og_sr_rx_make_ack(&s->rx,&ack)){
                if(radio_send(io,&ack))s->bitmap_acks_tx++;
                else {s->rx.ack_pending=true;s->rx.ack_due_ms=now+100;}
            }
            return true;
        }
    }
    if(s->rx_used && og_sr_rx_complete(&s->rx) && !s->rx_forwarded &&
       !s->rx_forwarding && !s->rx.ack_pending){
        s->rx_forwarding=true;s->rx_forward_offset=0;s->rx_seq=0;
    }
    if(s->rx_forwarding){
        /* USB forwarding no longer needs one radio receive poll per 160 B.
         * Bound output to four lines/turn (< 1600 serial bytes). The radio
         * worker remains the sole SX1278 owner and processes each RF IRQ. */
        for(unsigned i=0;i<OG_BRIDGE_USB_BATCH && s->rx_forwarding;i++)
            emit_rx(s,peer_mac,io);
        return true;
    }
    if(!s->tx_used || !allow_data_tx)return false;
    if(s->tx.phase==OG_SR_WAIT_ACK){
        uint32_t old=s->tx.deadline_ms;og_sr_tx_tick(&s->tx,now);
        if(s->tx.phase==OG_SR_MORE){s->ack_timeouts++;s->next_tx_at=now+45u+(entropy%170u);}
        else if(old==0)return false;
    }
    char id[17],line[112];transfer_id(s->tx.session,s->tx.transfer,id);
    if(s->tx.phase==OG_SR_FAILED){
        emit_failure(io,s->tx.session,s->tx.transfer,"BITMAP_RETRY_EXHAUSTED");
        s->tx_used=false;return true;
    }
    if(s->tx.phase==OG_SR_COMPLETE){
        const uint32_t completed_at=io->now_ms?io->now_ms(io->context):now;
        char stats[180];
        snprintf(stats,sizeof(stats),
          "BULKSTAT|%s|RF_MS=%" PRIu32 "|FRAMES=%" PRIu32 "|RETX=%" PRIu32 "|TIMEOUT=%" PRIu32,
          id,(uint32_t)(completed_at-s->transfer_started_ms),
          s->tx_frames-s->transfer_frames_base,
          s->retransmits-s->transfer_retries_base,
          s->ack_timeouts-s->transfer_timeouts_base);
        emit(io,stats);
        snprintf(line,sizeof(line),"BULKCONFIRMED|%s",id);emit(io,line);
        s->tx_used=false;return true;
    }
    if(s->tx.phase!=OG_SR_MORE||(int32_t)(now-s->next_tx_at)<0)return false;
    if(s->rx_used && !s->rx_forwarded && !og_sr_rx_complete(&s->rx) &&
       (int32_t)(now-s->rx_last_at)<(int32_t)(ack_silence_ms+120))return false;
    og_sr_frame_t frame={0};
    if(!og_sr_tx_pick(&s->tx,&frame))return false;
    const bool sent=radio_send(io,&frame);
    if(!sent){
        if(++s->tx_local_errors>=3){
            emit_failure(io,s->tx.session,s->tx.transfer,"RADIO_TX_FAILED");
            s->tx_used=false;
        }else s->next_tx_at=now+100+(entropy%140);
        return true;
    }
    s->tx_local_errors=0;s->tx_frames++;
    if(s->tx.attempts[frame.index])s->retransmits++;
    /* Use timestamp AFTER the blocking SX1278 TxDone callback. The old
     * stop-and-wait implementation used pre-Tx timestamps; that would cause
     * premature bitmap timeouts at SF9 and unnecessary retransmissions. */
    const uint32_t done_ms=io->now_ms?io->now_ms(io->context):now;
    s->tx_last_at=done_ms;
    if(s->tx_frames==s->transfer_frames_base+1u)s->first_frame_ms=done_ms;
    og_sr_tx_done(&s->tx,&frame,true,done_ms,ack_timeout_ms);
    if(!s->tx_sent_announced && s->tx.transmitted==bitmask(s->tx.fragments)){
        snprintf(line,sizeof(line),"BULKSENT|%s",id);emit(io,line);
        s->tx_sent_announced=true;
    }
    /* SF7/BW500 preamble spans only ~2 ms. The receiver needs time to
     * drain FIFO, verify HMAC, clear the IRQ, and rearm continuous RX.
     * The previous 3-ms gap could start the next preamble before it was
     * listening, causing a missing fragment and a costly bitmap timeout.
     * 25 ms is a cautious scheduling guard, NOT an airtime optimization. */
    s->next_tx_at=s->tx_last_at+OG_BRIDGE_INTERFRAME_GUARD_MS;
    return true;
}
