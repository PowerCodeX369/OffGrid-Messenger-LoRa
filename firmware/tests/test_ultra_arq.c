/* Host integration for 32-bit bitmap ACK with SF12 authenticated variable
 * fragment sizes. Does not claim real ESP32/SX1278 RF operation. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "og_arq_bridge.h"
#include "og_ultra_policy.h"
typedef struct device device;
struct device {
    og_arq_bridge_t b;
    og_arq_io_t io;
    device *other;
    uint32_t now;
    uint8_t queue[128][OG_SR_WIRE_MAX];size_t lengths[128];unsigned qhead,qtail;
    uint8_t receive[OG_SR_MAX_TRANSFER];size_t rxlen;
    unsigned complete,fail,rx_done,dropped_data,dropped_ack,retransmissions;
    bool drop_data,drop_ack;
};
static bool sign(void *ctx,const uint8_t *d,size_t n,uint8_t tag[16]) {
    (void)ctx;uint8_t x[16]={0};for(size_t i=0;i<n;i++)x[i%16]^=(uint8_t)(d[i]+i);
    memcpy(tag,x,16);return true;
}
static bool verify(void *ctx,const uint8_t *d,size_t n,const uint8_t tag[16]) {
    uint8_t expected[16];sign(ctx,d,n,expected);return !memcmp(tag,expected,16);
}
static bool radio(void *ctx,const uint8_t *d,size_t n) {
    device *a=ctx,*b=a->other;
    if(d[1]==OG_SR_DATA && a->drop_data && !a->dropped_data && d[3]==1){a->dropped_data++;return true;}
    if(d[1]==OG_SR_BITMAP_ACK && a->drop_ack && !a->dropped_ack){a->dropped_ack++;return true;}
    assert(b->qtail<128);
    memcpy(b->queue[b->qtail],d,n);b->lengths[b->qtail++]=n;return true;
}
static void line(void *ctx,const char *text){
    device *d=ctx;
    if(!strncmp(text,"BULKCONFIRMED|",14))d->complete++;
    if(!strncmp(text,"BULKFAILED|",11))d->fail++;
    if(!strncmp(text,"RXBSTART|",9))d->rxlen=0;
    if(!strncmp(text,"RXBCHUNK|",9)){
        const char *p=strrchr(text,'|')+1;assert(strlen(p)%2==0);
        for(size_t j=0;j<strlen(p)/2;j++){
            unsigned byte=0;assert(sscanf(p+2*j,"%2x",&byte)==1);
            assert(d->rxlen<sizeof(d->receive));d->receive[d->rxlen++]=(uint8_t)byte;
        }
    }
    if(!strncmp(text,"RXBDONE|",8))d->rx_done++;
}
static uint32_t ms(void *ctx){return ((device*)ctx)->now;}
static void setup(device *d) {
    og_arq_bridge_init(&d->b);
    d->io=(og_arq_io_t){.context=d,.radio_tx=radio,.serial_line=line,
        .now_ms=ms,.sign=sign,.verify=verify};
}
static void deliver(device *d){
    while(d->qhead<d->qtail){
        const uint8_t *wire=d->queue[d->qhead];size_t n=d->lengths[d->qhead++];
        if(wire[1]==OG_SR_DATA) {
            const uint8_t flags=wire[2];
            assert((flags&OG_SR_FLAG_ULTRA)!=0);
            assert(og_sr_payload_from_flags(flags)<=96);
        }
        og_arq_bridge_receive(&d->b,wire,n,d->now,350,&d->io);
    }
    d->qhead=d->qtail=0;
}
static void scenario(size_t size,uint8_t payload,bool drop_data,bool drop_ack){
    device *a=calloc(1,sizeof(*a)),*b=calloc(1,sizeof(*b));assert(a&&b);
    setup(a);setup(b);a->other=b;b->other=a;
    a->drop_data=drop_data;b->drop_ack=drop_ack;
    og_arq_bridge_set_ultra(&a->b,true,payload,payload==48?1u:2u);
    og_arq_bridge_set_ultra(&b->b,true,payload,2);
    uint8_t data[3072];for(size_t i=0;i<size;i++)data[i]=(uint8_t)(i*37u+11u);
    const uint32_t session=0xAABBCCDD,transfer=0x12345678;
    assert(og_arq_bridge_begin(&a->b,session,transfer,size,false,&a->io));
    for(size_t i=0,seq=0;i<size;i+=OG_BRIDGE_STAGE_CHUNK,seq++){
        size_t n=size-i;if(n>OG_BRIDGE_STAGE_CHUNK)n=OG_BRIDGE_STAGE_CHUNK;
        assert(og_arq_bridge_chunk(&a->b,session,transfer,(uint16_t)seq,data+i,n,&a->io));
    }
    assert(og_arq_bridge_end(&a->b,session,transfer,og_sr_usb_crc32(0,data,size),&a->io));
    assert(a->b.tx.fragment_payload==payload);
    assert(a->b.tx.fragments==(size+payload-1)/payload);
    for(uint32_t time=0;time<150000 && (!a->complete||!b->rx_done);time+=10){
        a->now=b->now=time;deliver(a);deliver(b);
        (void)og_arq_bridge_service(&a->b,time,time/19,350,3500,true,"00:11:22:33:44:55",&a->io);
        (void)og_arq_bridge_service(&b->b,time,time/23,350,3500,true,"00:11:22:33:44:55",&b->io);
    }
    assert(a->complete==1&&a->fail==0&&b->rx_done==1);
    assert(b->rxlen==size&&!memcmp(data,b->receive,size));
    assert(!drop_data||a->dropped_data==1);
    assert(!drop_ack||b->dropped_ack==1);
    if(drop_data||drop_ack)assert(a->b.retransmits>0);
    free(a);free(b);
}
int main(void){
    scenario(52,48,false,false);
    scenario(560,48,true,true);
    scenario(730,64,true,false);
    scenario(1980,80,false,true);
    scenario(2950,96,true,true);
    assert(og_ultra_airtime_ms(12,125000,5,8,34)>1200);
    assert(og_ultra_ack_wait_ms(34,0)>og_ultra_airtime_ms(12,125000,5,8,34));
    printf("PASS: Ultra SF12 authenticated selective ARQ with 48/64/80/96-byte fragments, lost data+ACK recovery and ciphertext integrity\n");
}
