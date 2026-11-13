#include "lora_auto.h"
#include "lora_wire.h"
#include <assert.h>
#include <stdio.h>
static void feed(og_auto_policy_t *p,int16_t rssi,int16_t snr,uint32_t now){
    for(int i=0;i<4;i++){
        og_auto_quality_note(&p->local,rssi,snr,now+100*i);
        og_auto_quality_note(&p->remote,rssi,snr,now+100*i);
    }
}
int main(void){
    og_auto_policy_t p;og_auto_reset(&p,100);
    feed(&p,-50,28,1000);
    assert(og_auto_decide(&p,OG_MODE_BALANCED,20000,false,false)==OG_MODE_BALANCED); /* warmup */
    assert(og_auto_decide(&p,OG_MODE_BALANCED,47500,false,false)==OG_MODE_BALANCED); /* stale */
    feed(&p,-80,20,47000);
    assert(og_auto_decide(&p,OG_MODE_BALANCED,48100,false,false)==OG_MODE_BALANCED);
    assert(og_auto_decide(&p,OG_MODE_BALANCED,52000,false,false)==OG_MODE_BALANCED);
    assert(og_auto_decide(&p,OG_MODE_BALANCED,56000,false,false)==OG_MODE_BALANCED);
    assert(og_auto_decide(&p,OG_MODE_BALANCED,60000,false,false)==OG_MODE_LOW);
    og_auto_reset(&p,61000);
    feed(&p,-110,-4,110000);
    assert(og_auto_decide(&p,OG_MODE_LOW,110900,true,false)==OG_MODE_LOW);
    assert(og_auto_decide(&p,OG_MODE_LOW,111000,false,false)==OG_MODE_LOW);
    assert(og_auto_decide(&p,OG_MODE_LOW,115000,false,false)==OG_MODE_BALANCED);
    og_auto_reset(&p,116000);
    feed(&p,-119,-24,162000);
    assert(og_auto_decide(&p,OG_MODE_BALANCED,163000,false,false)==OG_MODE_BALANCED);
    assert(og_auto_decide(&p,OG_MODE_BALANCED,167000,false,false)==OG_MODE_MAX);
    og_auto_reset(&p,168000);
    feed(&p,-93,24,215000);
    assert(og_auto_decide(&p,OG_MODE_MAX,216000,false,false)==OG_MODE_MAX);
    assert(og_auto_decide(&p,OG_MODE_MAX,220000,false,false)==OG_MODE_MAX);
    assert(og_auto_decide(&p,OG_MODE_MAX,224000,false,false)==OG_MODE_MAX);
    assert(og_auto_decide(&p,OG_MODE_MAX,228000,false,false)==OG_MODE_BALANCED);
    og_auto_reset(&p,230000);
    feed(&p,-110,-8,278000);
    p.remote.at_ms=1;
    assert(og_auto_decide(&p,OG_MODE_LOW,279000,false,true)==OG_MODE_LOW); /* stale peer -> no mode changes */
    og_auto_reset(&p,285000);
    feed(&p,-116,-20,286000);
    assert(og_auto_decide(&p,OG_MODE_LOW,288000,false,false)==OG_MODE_LOW);
    assert(og_auto_decide(&p,OG_MODE_LOW,292000,false,false)==OG_MODE_BALANCED);
    /* The signed OGM2 auto control and metric layouts are strict, so older
     * firmware cannot accidentally treat them as user plaintext. */
    og_frame_t a={.magic=OG_MAGIC,.version=OG_VERSION,.type=OG_TYPE_AUTO_CTRL,
                  .session=45,.id=0xAB120001u,.length=6};
    a.data[0]=1;a.data[1]=0xA5;
    a.data[2]=1;a.data[3]=0;a.data[4]=0x12;a.data[5]=0xAB;
    assert(og_wire_valid(&a,og_wire_length(&a)));
    a.type=OG_TYPE_AUTO_ACK;
    assert(og_wire_valid(&a,og_wire_length(&a)));
    a.data[1]=0;assert(!og_wire_valid(&a,og_wire_length(&a)));
    a.data[1]=0xA5;a.id++;
    assert(!og_wire_valid(&a,og_wire_length(&a)));
    a.id--;a.length=5;
    assert(!og_wire_valid(&a,og_wire_length(&a)));
    a.type=OG_TYPE_AUTO_METRIC;a.length=8;a.data[5]=OG_MODE_MAX;
    a.data[6]=1;a.data[7]=2;
    assert(og_wire_valid(&a,og_wire_length(&a)));
    a.data[5]=4;assert(!og_wire_valid(&a,og_wire_length(&a)));
    assert(OG_AUTO_FAST_UP_RSSI>OG_AUTO_FAST_DOWN_RSSI);
    assert(OG_AUTO_BAL_UP_RSSI>OG_AUTO_BAL_DOWN_RSSI);
    puts("PASS: two-direction quality, hysteresis, cooldown, promotion, downgrade, busy/stale safety");
}
