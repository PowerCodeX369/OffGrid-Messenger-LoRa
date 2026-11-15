#include "lora_auto.h"
#include "lora_wire.h"
#include <assert.h>
#include <stdio.h>
static void sample(og_auto_policy_t *p,int local,int remote,uint32_t now){
    og_auto_quality_note(&p->local,(int16_t)local,0,now);
    og_auto_quality_note(&p->remote,(int16_t)remote,0,now);
}
int main(void){
    og_auto_policy_t p;og_auto_reset(&p,100);
    /* Exact boundaries: above -100 BAL, -100 through -130 MAX, below -130 ULTRA. */
    assert(og_auto_rssi_band(-99)==OG_MODE_BALANCED);
    assert(og_auto_rssi_band(-100)==OG_MODE_MAX);
    assert(og_auto_rssi_band(-101)==OG_MODE_MAX);
    assert(og_auto_rssi_band(-129)==OG_MODE_MAX);
    assert(og_auto_rssi_band(-130)==OG_MODE_MAX);
    assert(og_auto_rssi_band(-131)==OG_MODE_ULTRA);
    assert(og_auto_rssi_band(-151)==OG_MODE_ULTRA);
    sample(&p,-99,-99,5000);
    assert(og_auto_decide(&p,OG_MODE_MAX,5000,false,false)==OG_MODE_BALANCED);
    sample(&p,-100,-100,5500);
    assert(og_auto_decide(&p,OG_MODE_BALANCED,5500,false,false)==OG_MODE_MAX);
    sample(&p,-130,-130,6000);
    assert(og_auto_decide(&p,OG_MODE_MAX,6000,false,false)==OG_MODE_MAX);
    sample(&p,-131,-131,6500);
    assert(og_auto_decide(&p,OG_MODE_MAX,6500,false,false)==OG_MODE_ULTRA);
    sample(&p,-130,-130,7000);
    assert(og_auto_decide(&p,OG_MODE_ULTRA,7000,false,false)==OG_MODE_MAX);
    sample(&p,-99,-99,7500);
    assert(og_auto_decide(&p,OG_MODE_MAX,7500,false,false)==OG_MODE_BALANCED);
    /* No Fast promotion, even at strongest RSSI. */
    sample(&p,-40,-40,8000);
    assert(og_auto_decide(&p,OG_MODE_BALANCED,8000,false,false)==OG_MODE_BALANCED);
    /* Worst-direction report is honored while recent, ignored when stale. */
    sample(&p,-96,-131,9000);
    assert(og_auto_decide(&p,OG_MODE_BALANCED,9000,false,false)==OG_MODE_ULTRA);
    og_auto_quality_note(&p.local,-95,0,30000);
    assert(og_auto_decide(&p,OG_MODE_ULTRA,30000,false,false)==OG_MODE_BALANCED);
    /* No new packets means no RSSI band decision, never invent -131. */
    assert(og_auto_decide(&p,OG_MODE_BALANCED,92000,false,false)==OG_MODE_BALANCED);
    assert(og_auto_decide_linkaware(&p,OG_MODE_BALANCED,30000,false,false,false)==OG_MODE_BALANCED);
    sample(&p,-132,-132,94000);
    assert(og_auto_decide_linkaware(&p,OG_MODE_BALANCED,94000,false,false,true)==OG_MODE_ULTRA);
    assert(og_auto_decide_linkaware(&p,OG_MODE_BALANCED,94000,true,false,true)==OG_MODE_BALANCED);
    /* Authentication telemetry never uses Bluetooth RSSI or SNR as a
     * threshold override, and does not smooth away a newly crossed band. */
    og_auto_quality_note(&p.local,-90,80,95000);
    og_auto_quality_note(&p.local,-131,80,95100);
    assert(p.local.rssi_dbm==-131);
    /* Deep SF12 fades can have valid -26 dB SNR; do not discard their
     * authenticated RSSI merely because the old parser capped SNR at -24. */
    og_auto_quality_note(&p.local,-135,-104,95200);
    assert(p.local.rssi_dbm==-135);
    assert(og_auto_decide(&p,OG_MODE_MAX,95200,false,false)==OG_MODE_ULTRA);
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
    a.data[5]=5;assert(!og_wire_valid(&a,og_wire_length(&a)));
    assert(OG_AUTO_FAST_DOWN_RSSI > OG_AUTO_FAST_RANGE_RSSI);
    assert(OG_AUTO_FAST_RANGE_RSSI >= OG_AUTO_BAL_DOWN_RSSI);
    assert(OG_AUTO_FAST_UP_RSSI>OG_AUTO_FAST_DOWN_RSSI);
    assert(OG_AUTO_BAL_UP_RSSI==OG_AUTO_BAL_DOWN_RSSI); /* no hysteresis per requested exact band */
    puts("PASS: exact -100/-130 LoRa bands, instant reverse, stale/busy/consent safety, authenticated wire frames");
}
