#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "lora_schedule.h"
int main(void){
    for(unsigned attempt=0;attempt<12;attempt++){
        for(uint32_t entropy=0;entropy<500;entropy++){
            uint32_t b=og_busy_backoff_ms(entropy,attempt,0x3C);
            assert(b>=65 && b<65+190+8*35+17);
            uint32_t retry=og_air_retry_jitter_ms(entropy,attempt,0x3C);
            assert(retry>=110 && retry<110+235+6*60+29);
        }
    }
    assert(og_busy_backoff_ms(0,0,0)!=og_busy_backoff_ms(0,0,1));
    puts("PASS: bounded, randomized and device-skewed LoRa contention schedule");
    return 0;
}
