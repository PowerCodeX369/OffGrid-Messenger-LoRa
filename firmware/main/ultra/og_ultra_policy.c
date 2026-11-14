#include "og_ultra_policy.h"
#include <limits.h>

static uint32_t ceil_div_u32(uint32_t a, uint32_t b) {
    return (a+b-1u)/b;
}

bool og_ultra_allows_content(uint8_t profile, og_content_kind_t kind) {
    if(profile<1u || profile>OG_ULTRA_MODE)return false;
    if(profile != OG_ULTRA_MODE)return kind>=OG_CONTENT_TEXT && kind<=OG_CONTENT_PROFILE_CONTROL;
    switch(kind){
    case OG_CONTENT_TEXT:
    case OG_CONTENT_TEXT_FRAG:
    case OG_CONTENT_DELIVERED_RECEIPT:
    case OG_CONTENT_READ_RECEIPT:
    case OG_CONTENT_AUTH_CONTROL:
    case OG_CONTENT_PROFILE_CONTROL:
        return true;
    default: return false;
    }
}

uint32_t og_ultra_airtime_ms(uint8_t sf,uint32_t bw,uint8_t cr,
                            uint16_t preamble,uint16_t physical_bytes){
    if(sf<7u||sf>12u||bw<7800u||bw>500000u||cr<5u||cr>8u||
       preamble<6u||physical_bytes==0u||physical_bytes>255u)return 0u;
    const uint64_t symbol_us=((UINT64_C(1)<<sf)*UINT64_C(1000000)+bw-1u)/bw;
    const int low_rate=(symbol_us>16000u)?1:0;
    const int32_t numerator=8*(int32_t)physical_bytes-4*(int32_t)sf+28+16;
    const uint32_t denom=4u*(sf-2u*(uint32_t)low_rate);
    const uint32_t symbols=8u+(numerator>0 ?
        ceil_div_u32((uint32_t)numerator,denom)*(uint32_t)cr : 0u);
    const uint64_t quarter_symbols=(uint64_t)4u*symbols+
                                   (uint64_t)4u*preamble+17u; /* 4.25 */
    const uint64_t total_us=(quarter_symbols*symbol_us+3u)/4u;
    return total_us>UINT32_MAX*UINT64_C(1000)?0u:
           (uint32_t)((total_us+999u)/1000u);
}

uint32_t og_ultra_ack_wait_ms(uint16_t ack_air_bytes,uint8_t attempt){
    /* Mandatory SF12/BW125 low data rate optimization is included in airtime. */
    const uint32_t ack_air=og_ultra_airtime_ms(12,125000,5,8,ack_air_bytes);
    if(!ack_air)return 0u;
    if(attempt>5u)attempt=5u;
    /* Starts AFTER TxDone, so do not add original DATA airtime twice. */
    return ack_air+1250u+350u*(uint32_t)attempt;
}

uint32_t og_ultra_fragment_deadline_ms(uint16_t payload_air_bytes,
                                      uint16_t ack_air_bytes,uint8_t attempt){
    uint32_t tx=og_ultra_airtime_ms(12,125000,5,8,payload_air_bytes);
    uint32_t ack=og_ultra_ack_wait_ms(ack_air_bytes,attempt);
    if(!tx||!ack||tx>UINT32_MAX-ack)return 0u;
    return tx+ack;
}

static bool valid(const og_link_quality_t *q){
    return q&&q->authenticated&&q->fresh&&q->losses_last_8<=8u;
}
static bool weak(const og_link_quality_t *q){
    return q->rssi_dbm<=OG_ULTRA_DOWN_RSSI_DBM ||
           q->snr_q4<=OG_ULTRA_DOWN_SNR_Q4 || q->losses_last_8>=3u;
}
static bool severe(const og_link_quality_t *q){
    return q->rssi_dbm<=-127 || q->snr_q4<=-48 || q->losses_last_8>=5u;
}
static bool strong(const og_link_quality_t *q){
    return q->rssi_dbm>=OG_ULTRA_UP_RSSI_DBM &&
           q->snr_q4>=OG_ULTRA_UP_SNR_Q4 && q->losses_last_8==0u;
}

uint16_t og_ultra_max_air_frame(const og_link_quality_t *a,
                               const og_link_quality_t *b){
    if(!valid(a)||!valid(b))return OG_ULTRA_FRAG_SEVERE_RADIO_BYTES;
    if(severe(a)||severe(b))return OG_ULTRA_FRAG_SEVERE_RADIO_BYTES;
    if(weak(a)||weak(b))return OG_ULTRA_FRAG_WEAK_RADIO_BYTES;
    return OG_ULTRA_MAX_RADIO_BYTES;
}

uint16_t og_ultra_fragment_data_limit(const og_link_quality_t *a,
                                     const og_link_quality_t *b,uint16_t overhead){
    uint16_t frame=og_ultra_max_air_frame(a,b);
    return overhead<frame?(uint16_t)(frame-overhead):0u;
}

uint8_t og_ultra_ack_window(const og_link_quality_t *a,
                            const og_link_quality_t *b){
    if(!valid(a)||!valid(b)||severe(a)||severe(b))return 1u;
    if(weak(a)||weak(b))return 2u;
    return 3u;
}

uint16_t og_ultra_fragment_count(uint16_t encrypted_bytes,uint16_t limit){
    if(!encrypted_bytes || !limit)return 0u;
    return (uint16_t)(((uint32_t)encrypted_bytes+(uint32_t)limit-1u)/limit);
}

uint32_t og_ultra_estimated_rf_ms(uint16_t encrypted_bytes,uint16_t overhead,
                                 uint16_t ack_air_bytes,
                                 const og_link_quality_t *a,const og_link_quality_t *b){
    uint16_t limit=og_ultra_fragment_data_limit(a,b,overhead);
    uint16_t parts=og_ultra_fragment_count(encrypted_bytes,limit);
    if(!parts)return 0u;
    uint64_t total=0;
    uint32_t left=encrypted_bytes;
    for(uint16_t i=0;i<parts;i++){
        const uint16_t part=(uint16_t)(left>limit?limit:left);
        const uint32_t air=og_ultra_airtime_ms(OG_ULTRA_SF,OG_ULTRA_BW_HZ,
                                  OG_ULTRA_CR_DENOM,OG_ULTRA_PREAMBLE,
                                  (uint16_t)(part+overhead));
        if(!air)return 0u;
        total+=air;left-=part;
    }
    const uint8_t window=og_ultra_ack_window(a,b);
    const uint32_t ack=og_ultra_ack_wait_ms(ack_air_bytes,0);
    if(!ack)return 0u;
    total+=(uint64_t)((parts+window-1u)/window)*ack;
    return total>UINT32_MAX?0u:(uint32_t)total;
}

bool og_ultra_should_request(uint8_t current_mode,
                             const og_link_quality_t *a,const og_link_quality_t *b,
                             og_ultra_decider_t *s){
    if(!s||current_mode!=OG_MAX_RANGE_MODE)return false;
    if(!valid(a)||!valid(b)){s->down_streak=0u;return false;}
    if(weak(a)||weak(b)){
        if(s->down_streak<255u)s->down_streak++;
    }else s->down_streak=0u;
    if(s->down_streak>=2u){s->down_streak=0u;s->up_streak=0u;return true;}
    return false;
}

bool og_ultra_should_recover(uint8_t current_mode,
                            const og_link_quality_t *a,const og_link_quality_t *b,
                            og_ultra_decider_t *s,uint32_t now_ms){
    if(!s||current_mode!=OG_ULTRA_MODE)return false;
    if(!valid(a)||!valid(b)){s->up_streak=0u;return false;}
    if(strong(a)&&strong(b)){
        if(s->up_streak<255u)s->up_streak++;
    }else s->up_streak=0u;
    if(s->up_streak>=5u&&(uint32_t)(now_ms-s->last_change_ms)>=120000u){
        s->down_streak=s->up_streak=0u;
        return true;
    }
    return false;
}
