/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "lora_profile_verify_timing.h"
#include "og_ultra_policy.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
int main(void){
    const uint32_t hello=og_ultra_airtime_ms(12,125000,8,8,22+16);
    assert(hello>2700u && hello<2900u); /* 38-byte HMAC HELLO CR4/8 */
    const uint32_t first_a=og_verify_first_delay_ms(4,true);
    const uint32_t first_b=og_verify_first_delay_ms(4,false);
    assert(first_a+hello+900u<first_b); /* follower must still be listening */
    const uint32_t again=og_verify_repeat_after_tx_ms(4);
    assert(first_b+hello+900u<first_a+hello+again);
    /* Repeated half-duplex slots never overlap for at least 4 exchanges. */
    uint32_t a=first_a,b=first_b;
    for(int i=0;i<4;i++){
        assert(a+hello+900u<b);
        uint32_t next_a=a+hello+again;
        assert(b+hello+900u<next_a);
        a=next_a;
        b=b+hello+again;
    }
    /* The listener's reply after receiving the leader must be finishable
       before the leader sends again. */
    assert(first_a+hello+og_verify_assist_after_rx_ms(4,false)+hello+900u
           <first_a+hello+again);
    assert(og_verify_first_delay_ms(2,true)==0);
    assert(og_verify_first_delay_ms(3,false)==0);
    assert(og_verify_repeat_after_tx_ms(3)==650);
    assert(og_verify_assist_after_rx_ms(2,false)==120);
    puts("PASS: separate SF12 initiator/responder verification slots avoid HELLO collisions");
}
