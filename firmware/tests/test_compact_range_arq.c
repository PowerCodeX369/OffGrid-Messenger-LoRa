/* SPDX-License-Identifier: GPL-3.0-or-later
 * Host simulation of real two-node authenticated 16-byte implicit ACKs.
 * The MAC callback is a test fixture; production uses PSA HMAC-SHA256-128.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "og_arq_bridge.h"
typedef struct radio_node radio_node;
struct radio_node {
    og_arq_bridge_t arq;
    og_arq_io_t io;
    radio_node *other;
    uint32_t now;
    uint8_t queue[160][OG_SR_WIRE_MAX];size_t size[160];unsigned head,tail;
    unsigned confirmed,failed,rx_done,compact_acks,legacy_acks,data_frames;
    unsigned drop_data,drop_ack,dropped_data,dropped_ack;
    uint8_t received[OG_SR_MAX_TRANSFER];size_t received_n;
};
static bool sign_test(void *ctx,const uint8_t *data,size_t n,uint8_t tag[16]){
    (void)ctx;uint8_t a[16]={0};
    for(size_t i=0;i<n;i++)a[i%16]^=(uint8_t)(data[i]+(i*7u));
    memcpy(tag,a,16);return true;
}
static bool verify_test(void *ctx,const uint8_t *data,size_t n,const uint8_t tag[16]){
    uint8_t expected[16];sign_test(ctx,data,n,expected);
    return memcmp(expected,tag,16)==0;
}
static bool radio_send(void *ctx,const uint8_t *bytes,size_t n){
    radio_node *a=ctx,*b=a->other;
    assert(n && n<=OG_SR_WIRE_MAX);
    if(n==OG_SR_TAG){
        a->compact_acks++;
        if(a->drop_ack && !a->dropped_ack){a->dropped_ack++;return true;}
    }else{
        if(bytes[1]==OG_SR_BITMAP_ACK)a->legacy_acks++;
        if(bytes[1]==OG_SR_DATA){
            a->data_frames++;
            if(a->drop_data && !a->dropped_data && bytes[3]==1){a->dropped_data++;return true;}
        }
    }
    assert(b->tail<160);
    memcpy(b->queue[b->tail],bytes,n);b->size[b->tail++]=n;
    return true;
}
static void serial_line(void *ctx,const char *line){
    radio_node *n=ctx;
    if(!strncmp(line,"BULKCONFIRMED|",14))n->confirmed++;
    if(!strncmp(line,"BULKFAILED|",11))n->failed++;
    if(!strncmp(line,"RXBSTART|",9))n->received_n=0;
    if(!strncmp(line,"RXBCHUNK|",9)){
        const char *p=strrchr(line,'|')+1;
        assert(strlen(p)%2==0);
        for(size_t i=0;i<strlen(p)/2;i++){
            unsigned b;assert(sscanf(p+i*2,"%2x",&b)==1);
            assert(n->received_n<sizeof(n->received));
            n->received[n->received_n++]=(uint8_t)b;
        }
    }
    if(!strncmp(line,"RXBDONE|",8))n->rx_done++;
}
static uint32_t clock_ms(void *ctx){return ((radio_node*)ctx)->now;}
static void init(radio_node *n){
    og_arq_bridge_init(&n->arq);
    n->io=(og_arq_io_t){.context=n,.radio_tx=radio_send,.serial_line=serial_line,
        .now_ms=clock_ms,.sign=sign_test,.verify=verify_test};
    og_arq_bridge_set_range_mode(&n->arq,true);
}
static void drain(radio_node *n){
    for(;n->head<n->tail;n->head++){
        const uint8_t *p=n->queue[n->head];size_t len=n->size[n->head];
        (void)og_arq_bridge_receive(&n->arq,p,len,n->now,1200,&n->io);
    }
    n->head=n->tail=0;
}
static void transfer(size_t bytes,uint8_t fragment,bool lose_data,bool lose_ack){
    radio_node *a=calloc(1,sizeof(*a)),*b=calloc(1,sizeof(*b));assert(a&&b);
    init(a);init(b);a->other=b;b->other=a;
    og_arq_bridge_set_range_text_payload(&a->arq,fragment);
    assert(a->arq.compact_ack_active && b->arq.compact_ack_active);
    a->drop_data=lose_data;b->drop_ack=lose_ack;
    const uint32_t session=0x12345678,tid=0x9abcdef0;
    uint8_t input[OG_SR_MAX_TRANSFER];
    assert(bytes<=sizeof(input));
    for(size_t i=0;i<bytes;i++)input[i]=(uint8_t)(i*37u+13u);
    assert(og_arq_bridge_begin(&a->arq,session,tid,bytes,false,&a->io));
    for(size_t at=0,seq=0;at<bytes;at+=OG_BRIDGE_STAGE_CHUNK,seq++){
        const size_t n=(bytes-at)>OG_BRIDGE_STAGE_CHUNK?OG_BRIDGE_STAGE_CHUNK:bytes-at;
        assert(og_arq_bridge_chunk(&a->arq,session,tid,(uint16_t)seq,input+at,n,&a->io));
    }
    assert(og_arq_bridge_end(&a->arq,session,tid,og_sr_usb_crc32(0,input,bytes),&a->io));
    assert(a->arq.tx.fragment_payload==fragment);
    for(uint32_t now=0;now<160000 && (!a->confirmed||!b->rx_done);now+=10){
        a->now=b->now=now;
        drain(a);drain(b);
        (void)og_arq_bridge_service(&a->arq,now,now/37,1200,1150,true,"PEER",&a->io);
        (void)og_arq_bridge_service(&b->arq,now,now/19,1200,1150,true,"PEER",&b->io);
    }
    if(a->confirmed!=1 || b->rx_done!=1)
        fprintf(stderr,"FAILED bytes=%zu frag=%u ack=%u rx=%u\n",bytes,fragment,a->confirmed,b->rx_done);
    assert(a->confirmed==1 && b->rx_done==1 && !a->failed);
    assert(b->received_n==bytes && !memcmp(b->received,input,bytes));
    assert(!a->legacy_acks && !b->legacy_acks && b->compact_acks>0);
    if(lose_ack)assert(b->dropped_ack==1 && a->arq.retransmits>0);
    if(lose_data)assert(a->dropped_data==1 && a->arq.retransmits>0);
    free(a);free(b);
}
static void forged_ack_cannot_confirm(void){
    og_sr_tx_t t={0};uint8_t payload[300]={0};og_sr_frame_t data={0},ack={0};
    assert(og_sr_tx_init_ex(&t,0xAAAABBBB,0xCCCCDDDD,payload,sizeof(payload),3,128));
    for(unsigned i=0;i<3;i++){
        assert(og_sr_tx_pick(&t,&data));
        og_sr_tx_done(&t,&data,true,100,1500);
    }
    og_sr_frame_t forged={.type=OG_SR_BITMAP_ACK,.session=t.session,
        .transfer=t.transfer,.count=t.fragments,.bitmap=7,.length=OG_SR_ACK_BYTES};
    uint8_t tag[16];assert(og_sr_compact_ack_encode(&forged,tag,sign_test,NULL));
    assert(og_sr_compact_ack_decode(tag,&t,&ack,verify_test,NULL));
    assert(ack.bitmap==7);
    tag[0]^=1;assert(!og_sr_compact_ack_decode(tag,&t,&ack,verify_test,NULL));
    tag[0]^=1;
    t.transfer++;
    assert(!og_sr_compact_ack_decode(tag,&t,&ack,verify_test,NULL));
    t.transfer--;
    t.session++;
    assert(!og_sr_compact_ack_decode(tag,&t,&ack,verify_test,NULL));
    t.session--;
    /* Replaying a previous confirmation cannot advance the next window. */
    assert(og_sr_tx_ack(&t,&forged));
    assert(!og_sr_compact_ack_decode(tag,&t,&ack,verify_test,NULL));
}
int main(void){
    transfer(50,128,false,false);
    transfer(320,128,true,true);
    transfer(2000,128,false,true);
    transfer(5000,224,true,true); /* MAX photo/voice keeps original capacity */
    forged_ack_cannot_confirm();
    puts("PASS: Max SF9 16-byte HMAC ACK, 128-byte text frames, media 224, lost DATA/ACK, tamper and replay rejection");
}
