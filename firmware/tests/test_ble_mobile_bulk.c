#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ble_command_policy.h"
int main(void){
 const char *accepted[]={"STATUS","KEYSTATUS","BULKBEGIN 0011223344556677 7136 DATA",
 "BULKCHUNK 0011223344556677 0 AABBCCDD","BULKEND 0011223344556677 00FF00FF",
 "CTRLHEX 0A0B","MSGHEX 0A0B","APPACK 00:11:22:33:44:55 1 2",
 "PROFILE LOW","PROFILE BALANCED","PROFILE MAX","AUTO ON","AUTO OFF",
 "POWER18 VERIFIED","POWER18 OFF","MEDIA ON","MEDIA OFF",NULL};
 for(int i=0;accepted[i];i++)assert(offgrid_ble_allows_command(accepted[i]));
 const char *rejected[]={"","KEYGET","KEYSET 0000","KEYGET EXTRA", "ESP_NOW SEND",
 "WIFI ON","POWER18 VERIFIED NOW","MEDIA ON;KEYGET",NULL};
 for(int i=0;rejected[i];i++)assert(!offgrid_ble_allows_command(rejected[i]));
 char oversize[520];memset(oversize,'A',sizeof(oversize));oversize[519]=0;
 assert(!offgrid_ble_allows_command(oversize));
 puts("PASS: authenticated BLE LoRa bulk/media/profile allowlist, key export and injection blocked");
 return 0;
}
