#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include "og_ultra_policy.h"

int main(void){
  /* SF12/BW125 symbol 32.768 ms; DE mandatory. Good-packet airtime verifies
   * Semtech Rev7 §4.1.1.7, explicit header and CRC, 8 preamble, CR4/5 baseline and CR4/8 Ultra. */
  assert(OG_ULTRA_MODEM_CONFIG1==0x78u && OG_ULTRA_MODEM_CONFIG2==0xC4u);
  assert(OG_ULTRA_MODEM_CONFIG3==0x0Cu);
  assert(OG_ULTRA_CR_DENOM==8u);
  assert(og_ultra_airtime_ms(12,125000,8,8,64)>
         og_ultra_airtime_ms(12,125000,5,8,64));
  assert(og_ultra_airtime_ms(12,125000,5,8,64)==2794);
  assert(og_ultra_airtime_ms(12,125000,5,8,38)==1975);
  assert(og_ultra_airtime_ms(12,125000,5,8,0)==0);
  assert(og_ultra_airtime_ms(13,125000,5,8,38)==0);
  assert(og_ultra_airtime_ms(12,125000,5,8,256)==0);
  assert(og_ultra_airtime_ms(12,125000,5,8,112)>
         og_ultra_airtime_ms(12,125000,5,8,64));
  assert(og_ultra_airtime_ms(12,125000,5,8,64)>
         og_ultra_airtime_ms(9,125000,5,8,64));
  assert(og_ultra_ack_wait_ms(38,0)>1975);
  assert(og_ultra_ack_wait_ms(38,3)>og_ultra_ack_wait_ms(38,0));
  assert(og_ultra_fragment_deadline_ms(64,38,0)>
         og_ultra_ack_wait_ms(38,0));
  for(int i=0;i<1000;i++){
    uint32_t tx=og_ultra_airtime_ms(12,125000,5,8,(uint16_t)(1+i%200));
    assert(tx>=og_ultra_airtime_ms(12,125000,5,8,1));
  }
  for(unsigned mode=1;mode<=4;mode++)for(int kind=1;kind<=9;kind++){
    bool expected=(mode!=4)||kind<=3||kind==6||kind==7||kind==9;
    assert(og_ultra_allows_content(mode,(og_content_kind_t)kind)==expected);
  }
  assert(!og_ultra_allows_content(0,OG_CONTENT_TEXT));
  assert(!og_ultra_allows_content(5,OG_CONTENT_TEXT));
  assert(!og_ultra_allows_content(4,(og_content_kind_t)99));
  og_link_quality_t a={true,true,-110,0,0},b={true,true,-110,0,0};
  og_ultra_decider_t s={0};
  assert(!og_ultra_should_request(OG_MAX_RANGE_MODE,&a,&b,&s));
  a.rssi_dbm=-117;
  assert(!og_ultra_should_request(OG_ULTRA_MODE,&a,&b,&s));
  assert(!og_ultra_should_request(OG_MAX_RANGE_MODE,&a,&b,&s));
  assert(og_ultra_should_request(OG_MAX_RANGE_MODE,&a,&b,&s));
  assert(!og_ultra_should_request(OG_MAX_RANGE_MODE,&a,&b,&s));
  a.fresh=false;
  assert(!og_ultra_should_request(OG_MAX_RANGE_MODE,&a,&b,&s));
  a.fresh=true;a.authenticated=false;
  assert(!og_ultra_should_request(OG_MAX_RANGE_MODE,&a,&b,&s));
  a.authenticated=true;a.rssi_dbm=-108;a.snr_q4=-24;
  assert(!og_ultra_should_request(OG_MAX_RANGE_MODE,&a,&b,&s));
  assert(og_ultra_should_request(OG_MAX_RANGE_MODE,&a,&b,&s));
  a.snr_q4=0;a.losses_last_8=5;
  assert(og_ultra_max_air_frame(&a,&b)==56);
  assert(og_ultra_fragment_data_limit(&a,&b,38)==18);
  assert(og_ultra_ack_window(&a,&b)==1);
  assert(og_ultra_fragment_count(53,18)==3);
  assert(og_ultra_estimated_rf_ms(53,38,38,&a,&b)>12000);
  a.losses_last_8=3;
  assert(og_ultra_max_air_frame(&a,&b)==72);
  assert(og_ultra_fragment_data_limit(&a,&b,38)==34);
  assert(og_ultra_ack_window(&a,&b)==2);
  assert(og_ultra_fragment_count(53,34)==2);
  a.losses_last_8=0;a.rssi_dbm=-90;
  assert(og_ultra_max_air_frame(&a,&b)==112);
  assert(og_ultra_fragment_data_limit(&a,&b,38)==74);
  assert(og_ultra_ack_window(&a,&b)==3);
  assert(og_ultra_fragment_count(53,74)==1);
  assert(og_ultra_fragment_count(0,74)==0);
  assert(og_ultra_fragment_count(100,0)==0);
  assert(og_ultra_estimated_rf_ms(0,38,38,&a,&b)==0);
  assert(og_ultra_estimated_rf_ms(53,38,38,&a,&b)>4300);
  a.rssi_dbm=-97;a.snr_q4=16;b.rssi_dbm=-97;b.snr_q4=16;
  s.last_change_ms=100000;
  assert(!og_ultra_should_recover(OG_MAX_RANGE_MODE,&a,&b,&s,225000));
  for(int i=0;i<4;i++)assert(!og_ultra_should_recover(OG_ULTRA_MODE,&a,&b,&s,225000));
  assert(og_ultra_should_recover(OG_ULTRA_MODE,&a,&b,&s,225000));
  assert(!og_ultra_should_recover(OG_ULTRA_MODE,&a,&b,&s,230000));
  s.last_change_ms=300000;
  for(int i=0;i<7;i++) assert(!og_ultra_should_recover(OG_ULTRA_MODE,&a,&b,&s,302000));
  printf("PASS: SF12/BW125 airtime, LDRO, content gate, authenticated two-way switch, adaptive frame limits, ACK timeouts\n");
  return 0;
}
