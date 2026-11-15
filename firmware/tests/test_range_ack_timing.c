#include <assert.h>
#include <stdio.h>
#include "lora_arq_timing.h"
int main(void){
    const uint32_t ultra_next=og_ultra_airtime_ms(12,125000,8,8,48+OG_SR_HEADER+OG_SR_TAG);
    const uint32_t range_next=og_ultra_airtime_ms(9,125000,5,8,128+OG_SR_HEADER+OG_SR_TAG);
    assert(ultra_next>350 && range_next>400);
    assert(og_lora_arq_silence_ms(OG_MODE_ULTRA,48)>=ultra_next+180);
    assert(og_lora_arq_silence_ms(OG_MODE_MAX,128)>=range_next+180);
    assert(og_lora_arq_silence_ms(OG_MODE_MAX,224)>og_lora_arq_silence_ms(OG_MODE_MAX,128));
    assert(og_lora_arq_ack_rto_ms(OG_MODE_ULTRA,0)>
           og_ultra_airtime_ms(12,125000,8,8,16));
    assert(og_lora_arq_ack_rto_ms(OG_MODE_MAX,0)>
           og_ultra_airtime_ms(9,125000,5,8,16));
    assert(og_lora_arq_ack_rto_ms(OG_MODE_BALANCED,0)==750);
    assert(og_lora_arq_ack_rto_ms(OG_MODE_LOW,0)==420);
    printf("PASS: SF9/SF12 partial ACK waits through next DATA airtime; 16-byte authenticated ACK RTO; Fast/Balanced frozen\n");
}
