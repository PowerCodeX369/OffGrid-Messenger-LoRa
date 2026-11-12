/* Deterministic 2-node host integration of the actual firmware ARQ bridge.
 * Verifies real USB staging contract and selective bitmap loss recovery. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "og_arq_bridge.h"

typedef struct node node;
struct node {
    og_arq_bridge_t b;
    og_arq_io_t io;
    node *peer;
    uint32_t now,session,transfer;
    uint8_t packets[200][OG_SR_WIRE_MAX];
    size_t sizes[200];
    unsigned head,tail,drop_data,drop_index,drop_ack,dropped_data,dropped_ack;
    unsigned rx_count,rx_bytes,confirmed,sent,failed;
    uint8_t received[OG_SR_MAX_TRANSFER];
};
static bool sign(void *c,const uint8_t *bytes,size_t n,uint8_t out[16]){
    (void)c;uint8_t hash[16]={0};
    for(size_t i=0;i<n;i++)hash[i%16]^=(uint8_t)(bytes[i]+(i&255));
    memcpy(out,hash,16);return true;
}
static bool verify(void *c,const uint8_t *bytes,size_t n,const uint8_t tag[16]){
    uint8_t mac[16];(void)c;sign(NULL,bytes,n,mac);return memcmp(mac,tag,16)==0;
}
static bool radio(void *ctx,const uint8_t *wire,size_t n){
    node *a=ctx,*b=a->peer;
    assert(n<=OG_SR_WIRE_MAX && n>=34);
    if(wire[1]==OG_SR_DATA && a->drop_data && !a->dropped_data && wire[3]==a->drop_index){
        a->dropped_data=1;return true;
    }
    if(wire[1]==OG_SR_BITMAP_ACK && a->drop_ack && !a->dropped_ack){
        a->dropped_ack=1;return true;
    }
    assert(b->tail<200);
    memcpy(b->packets[b->tail],wire,n);b->sizes[b->tail++]=n;
    return true;
}
static void emit(void *ctx,const char *line){
    node *a=ctx;
    if(!strncmp(line,"BULKCONFIRMED|",14))a->confirmed++;
    if(!strncmp(line,"BULKSENT|",9))a->sent++;
    if(!strncmp(line,"BULKFAILED|",11))a->failed++;
    if(!strncmp(line,"RXBSTART|",9)){a->rx_bytes=0;a->rx_count=0;}
    if(!strncmp(line,"RXBCHUNK|",9)){
        const char *hex=strrchr(line,'|')+1;
        assert(strlen(hex)%2==0);
        for(size_t i=0;i<strlen(hex)/2;i++){
            unsigned b;assert(sscanf(hex+i*2,"%2x",&b)==1);
            assert(a->rx_bytes<OG_SR_MAX_TRANSFER);
            a->received[a->rx_bytes++]=(uint8_t)b;
        }
    }
    if(!strncmp(line,"RXBDONE|",8))a->rx_count++;
}
static uint32_t now_ms(void *ctx){return ((node*)ctx)->now;}
static void init(node *n){
    og_arq_bridge_init(&n->b);
    n->io=(og_arq_io_t){.context=n,.radio_tx=radio,.serial_line=emit,
        .now_ms=now_ms,.sign=sign,.verify=verify};
}
static void drain(node *n){
    while(n->head<n->tail){
        (void)og_arq_bridge_receive(&n->b,n->packets[n->head],n->sizes[n->head],
                                    n->now,100,&n->io);
        n->head++;
    }
    if(n->head==n->tail)n->head=n->tail=0;
}
static void scenario(size_t size,bool drop_data,bool drop_ack,bool drop_last){
    node *a=calloc(1,sizeof(*a)),*b=calloc(1,sizeof(*b));
    assert(a&&b);
    init(a);init(b);a->peer=b;b->peer=a;
    a->drop_data=drop_data;
    a->drop_index=drop_last?(unsigned)((size+OG_SR_PAYLOAD-1)/OG_SR_PAYLOAD-1):1;
    a->drop_ack=0;
    b->drop_ack=drop_ack;
    const uint32_t session=0x12345678u,transfer=0x9abcdef0u;
    uint8_t *bytes=malloc(size);assert(bytes);
    for(size_t i=0;i<size;i++)bytes[i]=(uint8_t)((i*71+13)&255);
    assert(og_arq_bridge_begin(&a->b,session,transfer,size,false,&a->io));
    for(size_t i=0,seq=0;i<size;i+=OG_BRIDGE_STAGE_CHUNK,seq++){
        size_t n=size-i;if(n>OG_BRIDGE_STAGE_CHUNK)n=OG_BRIDGE_STAGE_CHUNK;
        assert(og_arq_bridge_chunk(&a->b,session,transfer,(uint16_t)seq,bytes+i,n,&a->io));
    }
    assert(og_arq_bridge_end(&a->b,session,transfer,og_sr_usb_crc32(0,bytes,size),&a->io));
    for(unsigned now=0;now<130000 && (!a->confirmed || !b->rx_count);now+=10){
        a->now=b->now=now;
        drain(a);drain(b);
        (void)og_arq_bridge_service(&a->b,now,now/7,100,410,true,"PEER",&a->io);
        (void)og_arq_bridge_service(&b->b,now,now/11,100,410,true,"PEER",&b->io);
    }
    if(!a->confirmed || !b->rx_count)
        printf("DEBUG transfer=%lu ack=%u rx=%u txstate=%d frames=%u timeouts=%u failures=%u\n",
               (unsigned long)size,a->confirmed,b->rx_count,(int)a->b.tx.phase,
               a->b.tx_frames,a->b.ack_timeouts,a->failed);
    assert(a->confirmed==1 && a->sent==1 && b->rx_count==1 && a->failed==0);
    assert(b->rx_bytes==size && !memcmp(bytes,b->received,size));
    if(size==1024 && !drop_data && !drop_ack){
        /* Five 224-byte fragments now fit a single six-frame window.
         * One bitmap ACK replaces the old 4+1 two-ACK exchange. */
        assert(a->b.tx.fragments==5);
        assert(a->b.tx.window==6);
        assert(b->b.bitmap_acks_tx==1);
        assert(a->b.bitmap_acks_rx==1);
    }
    assert(a->b.next_tx_at>=a->b.tx_last_at+OG_BRIDGE_INTERFRAME_GUARD_MS);
    assert(!drop_data || (a->dropped_data && a->b.retransmits>0));
    assert(!drop_ack || (b->dropped_ack && a->b.ack_timeouts>0));
    free(bytes);free(a);free(b);
}
int main(void){
    scenario(1024,false,false,false);
    scenario(1024,true,true,false);
    scenario(4096,true,true,false);
    scenario(OG_SR_MAX_TRANSFER,true,true,false);
    scenario(1024,true,true,true); /* missing last: silence ACK then selective retry */
    scenario(OG_SR_MAX_TRANSFER,true,true,true);
    puts("PASS: actual bridge integration: 1K/4K/7168 B, lost frames+ACKs, bulk staging, reassembly");
    return 0;
}
