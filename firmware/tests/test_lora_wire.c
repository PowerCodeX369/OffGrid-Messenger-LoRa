#include "lora_wire.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    og_frame_t f={0};
    f.magic=OG_MAGIC;f.version=OG_VERSION;f.type=OG_TYPE_DATA;f.length=180;
    assert(sizeof(f)==OG_FRAME_MAX);
    assert(OG_AIR_MAX<=220);
    assert(og_wire_valid(&f,og_wire_length(&f)));
    assert(!og_wire_valid(&f,og_wire_length(&f)-1));
    assert(!og_wire_valid(&f,og_wire_length(&f)+1));
    f.length=181;assert(!og_wire_valid(&f,OG_FRAME_MAX));
    f.length=0;assert(!og_wire_valid(&f,OG_FRAME_HDR_BYTES));
    f.type=OG_TYPE_HELLO;assert(og_wire_valid(&f,OG_FRAME_HDR_BYTES));
    f.length=1;assert(!og_wire_valid(&f,OG_FRAME_HDR_BYTES+1));
    f.length=0;f.type=OG_TYPE_RADIO_ACK;assert(og_wire_valid(&f,OG_FRAME_HDR_BYTES));
    /* Regression: both v3.0.5 peers announce ARQ_CAPS=3, and older
     * validators accidentally rejected it. Every message failed before RF TX.
     * Test the actual frame validator used after HMAC authentication. */
    f.type=OG_TYPE_ARQ_CAPS;
    f.length=1;
    for(unsigned cap=1;cap<=4;cap++){
        f.data[0]=(uint8_t)cap;
        assert(og_wire_valid(&f,og_wire_length(&f)));
    }
    f.data[0]=0;assert(!og_wire_valid(&f,og_wire_length(&f)));
    f.data[0]=5;assert(!og_wire_valid(&f,og_wire_length(&f)));
    f.data[0]=255;assert(!og_wire_valid(&f,og_wire_length(&f)));
    f.length=0;f.data[0]=3;assert(!og_wire_valid(&f,og_wire_length(&f)));
    f.length=2;assert(!og_wire_valid(&f,og_wire_length(&f)));
    f.length=0;
    f.type=77;assert(!og_wire_valid(&f,OG_FRAME_HDR_BYTES));
    assert(og_ack_wait_ms(0)>=400);
    assert(og_ack_wait_ms(1)<800);
    assert(og_ack_wait_ms(4)<1500);
    assert(og_ack_wait_ms(1)<og_ack_wait_ms(3));
    puts("PASS: LoRa frame sizes, ARQ_CAPS v1/v2/v3/v4 authenticated compatibility, invalid caps rejected, ACK timing");
    return 0;
}
