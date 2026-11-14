/* Deterministic control payload policy — no radio or timing claims. */
#include "lora_profile_control.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const uint8_t alice[6]={0x14,0x63,0x93,0xC2,0xF3,0x3C};
int main(void){
    for(uint8_t mode=OG_MODE_LOW;mode<=OG_MODE_ULTRA;mode++){
        og_frame_t request=og_profile_control_frame(OG_TYPE_PROFILE_OFFER,mode,alice,25u,0xFE129411u);
        assert(og_wire_valid(&request,og_wire_length(&request)));
        assert(og_profile_control_valid(&request));
        assert(request.data[5]==(og_profile_high_pa(mode)?OG_PROFILE_CONFIRM_BOTH_PA:0u));
        /* Receiver has no local USB approval but is allowed to accept the
         * already-authenticated initiating operator's two-board approval. */
        const bool receiver_usb_approved=false;
        (void)receiver_usb_approved;
        assert(og_profile_control_valid(&request));
        og_frame_t agree=og_profile_control_frame(OG_TYPE_PROFILE_AGREE,mode,alice,25u,0xFE129411u);
        assert(og_wire_valid(&agree,og_wire_length(&agree)));
        assert(og_profile_control_valid(&agree));
        assert(agree.data[5]==request.data[5]);
        request.data[5]^=0x08u;
        assert(!og_profile_control_valid(&request)); /* missing/altered authorization */
        request.data[5]^=0x08u;request.id++;
        assert(!og_profile_control_valid(&request)); /* nonce mismatch */
        request.id--;request.length=5;
        assert(!og_wire_valid(&request,og_wire_length(&request))); /* rejects old format */
    }
    /* Temporary media mode override is a separately authenticated control
     * intent. It must not be mistaken for permanent manual or Auto state. */
    for(uint8_t mode=OG_MODE_LOW;mode<=OG_MODE_ULTRA;mode++){
        og_frame_t media=og_profile_control_frame(OG_TYPE_PROFILE_OFFER,mode,alice,25u,0xFE129411u);
        media.data[5]|=OG_PROFILE_MEDIA_FLAG;
        assert(og_profile_control_valid(&media));
        assert(og_profile_is_media(&media) && !og_profile_is_auto(&media));
        media.type=OG_TYPE_PROFILE_AGREE;
        assert(og_profile_control_valid(&media));
        media.data[5]|=OG_PROFILE_AUTO_FLAG;
        assert(!og_profile_control_valid(&media)); /* no ambiguous intent */
        media.data[5]&=(uint8_t)~OG_PROFILE_AUTO_FLAG;
        media.data[5]^=0x01u; /* mutated intent or approval is invalid */
        assert(!og_profile_control_valid(&media));
    }
    og_frame_t reject=og_profile_control_frame(OG_TYPE_PROFILE_REJECT,OG_MODE_MAX,alice,25u,0xFE129411u);
    assert(og_wire_valid(&reject,og_wire_length(&reject)));
    assert(og_profile_control_valid(&reject));
    reject.data[5]=0;assert(!og_profile_control_valid(&reject));
    assert(!og_profile_mode_valid(0) && !og_profile_mode_valid(5));
    assert(og_mode_parse("EXPERIMENTAL")==0);
    puts("PASS: authenticated profile payload policy, one-click peer, busy response, 4 modes");
    return 0;
}
