/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdbool.h>
#include <string.h>

/* Authenticated BLE commands compatible with the LoRa v1.8 serial protocol.
 * The caller MUST require a fresh OffGridBLE2 HMAC challenge/response before
 * forwarding any allowed command to the radio task. KEYGET/KEYSET are never
 * allowed over the air; the phone must be paired with a key provisioned via USB. */
static inline bool offgrid_ble_allows_command(const char *s){
 if(!s || strlen(s)>508u)return false;
 return !strcmp(s,"STATUS")||!strcmp(s,"KEYSTATUS")||
        !strncmp(s,"BULKBEGIN ",10)||!strncmp(s,"BULKCHUNK ",10)||!strncmp(s,"BULKEND ",8)||
        !strncmp(s,"CTRLHEX ",8)||!strncmp(s,"PROFILE ",8)||
        !strcmp(s,"AUTO ON")||!strcmp(s,"AUTO OFF")||
        !strcmp(s,"MEDIA ON")||!strcmp(s,"MEDIA OFF")||
        !strcmp(s,"POWER18 VERIFIED")||!strcmp(s,"POWER18 OFF")||
        !strncmp(s,"MSGHEX ",7)||!strncmp(s,"APPACK ",7)||
        !strncmp(s,"APPACKB ",8)||!strncmp(s,"READ ",5)||
        !strcmp(s,"RATE AUTO")||!strcmp(s,"RATE SPEED")||!strcmp(s,"RATE RANGE")||
        !strcmp(s,"RATE LR500")||!strcmp(s,"RATE LR250");
}
