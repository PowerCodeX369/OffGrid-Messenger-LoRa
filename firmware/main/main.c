/*
 * OffGrid Messenger LoRa v1.6.1-rc.2 / ESP32-C3 + SX1278 Ra-02
 *
 * No esp_wifi_init, no esp_now_init, no Wi-Fi event loop, no Wi-Fi tasks.
 * BLE (NimBLE) and native USB continue to serve the existing OGM4 client.
 * The single LoRa task owns ALL SX1278 SPI transactions and authenticated
 * MAC-layer retries. No BLE or USB task is allowed to touch the SX1278.
 *
 * Privacy: AES-256-GCM happens in the browser (unchanged original protocol).
 * All over-air OGM2 frames, including discovery and ACKs, are HMAC-SHA256
 * authenticated (truncated to 128 bits) with a domain-separated key.
 */
#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ble_bridge.h"
#include "lora_wire.h"
#include "lora_schedule.h"
#include "lora_modes.h"
#include "lora_profile_control.h"
#include "lora_profile_verify_timing.h"
#include "lora_auto.h"
#include "lora_link_recovery.h"
#include "ultra/og_ultra_policy.h"
#include "og_arq_bridge.h"
#include "offgrid_sx1278.h"
#include "esp_err.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "psa/crypto.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_idf_version.h"
#if CONFIG_OFFGRID_USB_IRQ_RX
#include "driver/usb_serial_jtag.h"
#if __has_include("driver/usb_serial_jtag_vfs.h")
#include "driver/usb_serial_jtag_vfs.h"
#define OG_USB_VFS_USE_DRIVER() usb_serial_jtag_vfs_use_driver()
#else
#include "esp_vfs_dev.h"
#define OG_USB_VFS_USE_DRIVER() esp_vfs_usb_serial_jtag_use_driver()
#endif
#endif

#define OG_EVENT_QUEUE 32
#define OG_PENDING 8
#define OG_RX_HISTORY 32
#define OG_CTRL_QUEUE 18
#define OG_SERIAL_BUF 512
#define OG_PEER_STALE_MS 30000
#define OG_TX_ATTEMPTS 4
#define OG_CTRL_ATTEMPTS 3
#define OG_STATUS_MS 2500
#define OG_HELLO_MS 12000
#define OG_DISCOVERY_HELLO_MS 6500
#define OG_RX_POLL_MS 24
#define OG_RADIO_TX_TIMEOUT_MS 2500
#define OG_ULTRA_CTRL_MIN_GAP_MS 1800u
#define OG_PROFILE_OFFER_RETRY_MS 750
#define OG_PROFILE_VERIFY_MS 6500
#define OG_PROFILE_SWITCH_GRACE_MS 600

/* LoRa application identity is NOT an unencrypted Wi-Fi network MAC.
 * ESP_MAC_WIFI_STA derives the base eFuse MAC without starting Wi-Fi. */
static uint8_t own_mac[6];
static uint32_t boot_session, next_id = 1;
static uint8_t pair_key[32];
static bool key_present = false;
static QueueHandle_t commands;
static bool usb_irq_rx = false;
/* This is a local USB activity hint, not a new RF protocol. Auto profile
 * negotiation still requires authenticated OFFER/AGREE from BOTH nodes.
 * Expiration guards against a crashed/disconnected web client. */
static int64_t media_active_until_ms;
/* A temporary media boost is a signed two-node PROFILE transaction. Keep
 * the user's manual setting AND Auto-enabled state unchanged. No RF payload
 * is sent until the new profile is authenticated on-air. */
static bool media_boost_active, media_requested_profile, media_restore_requested;
static uint8_t media_original_mode;
static uint8_t media_restore_attempts;
static int64_t media_restore_retry_ms;

static const uint8_t KDF_CONTEXT[] = "OffGrid-LoRa-HMAC-v1";

typedef enum { CMD_DATA=1, CMD_URGENT, CMD_APPACK, CMD_READ, CMD_STATUS,
               CMD_KEY, CMD_PROFILE, CMD_BULK_BEGIN, CMD_BULK_CHUNK,
               CMD_BULK_END, CMD_HIGH_PA_ON, CMD_HIGH_PA_OFF, CMD_AUTO_ON, CMD_AUTO_OFF, CMD_MEDIA_ON, CMD_MEDIA_OFF } og_cmd_type_t;
typedef struct {
    og_cmd_type_t type;
    uint8_t mac[6];
    uint32_t session, id;
    uint16_t length, ble_token;
    bool ble_binary;
    uint8_t profile_target;
    bool force_profile;
    uint32_t bulk_session, bulk_transfer, bulk_crc;
    bool bulk_urgent, bulk_media;
    uint8_t data[OG_MAX_DATA];
    uint8_t key[32];
} og_command_t;
typedef struct {
    bool used, radio_acked, urgent, tx_notified;
    og_frame_t frame;
    uint8_t tries;
    int64_t due_ms, tx_at_ms;
} og_pending_t;
typedef struct {
    bool used, app_acked;
    uint8_t sender[6];
    uint32_t session, id;
    int64_t forwarded_ms;
} og_received_t;
typedef struct {
    og_frame_t frame;
    int64_t due_ms;
} og_control_t;

/* Only the LoRa task accesses these. No cross-task volatile shortcuts. */
static og_pending_t pending[OG_PENDING];
static og_received_t received[OG_RX_HISTORY];
static size_t received_next;
static og_control_t controls[OG_CTRL_QUEUE];
static uint8_t peer_mac[6];
static bool peer_seen;
static int64_t peer_at_ms, last_hello_ms, last_link_ms, last_ready_ms;
static int16_t peer_rssi = -127;
static uint32_t stat_tx, stat_rx, stat_radio_ack, stat_delivered;
static uint32_t stat_retry, stat_fail, stat_badtag, stat_crc, stat_hello;
static uint8_t radio_key[32];
static bool radio_key_ready;
static psa_key_id_t radio_psa_id;
static uint32_t stat_tx_done,stat_appack,stat_tx_errors,stat_busy_cad;
static offgrid_sx1278_t *sx;
/* 3 bounded 7168-byte ciphertext buffers. No plaintext or keys in the ARQ
 * buffers. The radio task is the sole owner of the complete bridge. */
static og_arq_bridge_t arq;
static const og_arq_io_t arq_io;
static uint32_t arq_tx_done,arq_auth_rx;
static int64_t arq_last_tx_ms;
static bool high_pa_verified;
static bool peer_arq_seen;
static int64_t peer_arq_at_ms,last_arq_caps_ms;
static bool peer_ultra_capable; /* Only authenticated ARQ_CAPS=3 matches Ultra CR4/8. */

/* A profile change is explicitly user-initiated. Both radios exchange HMAC-
 * authenticated OGM2 control frames on the OLD profile before either switches.
 * If confirmation fails on the new profile, roll back to the previous mode. */
typedef enum { OG_PROFILE_IDLE, OG_PROFILE_OFFERING, OG_PROFILE_RESPONDING,
               OG_PROFILE_WAIT_SWITCH, OG_PROFILE_VERIFYING } og_profile_phase_t;
typedef struct {
    og_profile_phase_t phase;
    uint8_t current, target, previous, attempts;
    uint32_t nonce, offer_session;
    int64_t retry_at_ms, switch_at_ms, verify_deadline_ms, probe_at_ms;
} og_profile_change_t;
static og_profile_change_t profile_state={.current=OG_MODE_BALANCED};
static uint32_t stat_profile_changes, stat_profile_rollbacks;
/* Reject a replay of the last accepted authenticated switch request. A full
 * security audit will need persistent anti-replay state across reboots. */
static bool last_profile_offer_valid;
static uint32_t last_profile_offer_session,last_profile_offer_nonce;
static uint8_t last_profile_offer_mac[6];
/* After confirming a new profile, continue short authenticated HELLOs so the
 * other node can also confirm its profile and avoid asymmetrical rollback. */
static int64_t profile_assist_until_ms, profile_assist_next_ms;
static bool profile_verify_initiator; /* OFFER sender has first SF12 HELLO slot */
/* RF automation is opt-in and limited to the established single radio task.
 * A deterministic single leader (lower MAC) selects modes, based on recent
 * authenticated local + peer RSSI/SNR reports. The existing synchronized
 * OFFER/AGREE/verify/rollback protocol still performs the actual switch. */
static bool auto_enabled, auto_requested_profile;
static og_auto_policy_t auto_policy;
static bool auto_ctrl_pending, auto_ctrl_target;
static uint32_t auto_ctrl_nonce, auto_ctrl_session;
static uint8_t auto_ctrl_retries;
static int64_t auto_ctrl_retry_at, auto_last_metric_ms, auto_defer_until_ms;
static uint32_t auto_last_accepted_id, auto_last_accepted_session;
static bool auto_seen_control;
static uint32_t auto_last_metric_session, auto_last_metric_id;
static bool auto_peer_enabled;
static uint32_t auto_attempts, auto_failures;
static uint32_t auto_last_metric_loss_count;
static uint8_t auto_peer_recent_losses;
/* Recovery only touches SX1278 from the sole radio owner task. */
static og_recovery_t recovery;
static uint32_t recovery_locks, recovery_scans;
static int64_t auto_last_block_report_ms;
static int64_t last_recovery_rx_refresh_ms;





static int64_t millis(void) { return esp_timer_get_time()/1000LL; }
static void mac_text(const uint8_t m[6], char out[18]) {
    snprintf(out,18,"%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);
}
static bool mac_parse(const char *s, uint8_t out[6]) {
    unsigned n[6];int end=0;
    if (!s || sscanf(s,"%2x:%2x:%2x:%2x:%2x:%2x%n",&n[0],&n[1],&n[2],&n[3],&n[4],&n[5],&end)!=6 || s[end]) return false;
    for(int i=0;i<6;i++)out[i]=(uint8_t)n[i];
    return true;
}
static bool profile_has_pending(void) {
    if(og_arq_bridge_busy(&arq))return true;
    for(size_t i=0;i<OG_PENDING;i++)if(pending[i].used)return true;
    return false;
}
static og_frame_t profile_frame(uint8_t type, uint8_t target, uint32_t session, uint32_t nonce) {
    og_frame_t f=og_profile_control_frame(type,target,own_mac,session,nonce);
    if(type==OG_TYPE_PROFILE_OFFER || type==OG_TYPE_PROFILE_AGREE){
        if(media_requested_profile)f.data[5]|=OG_PROFILE_MEDIA_FLAG;
        else if(auto_requested_profile)f.data[5]|=OG_PROFILE_AUTO_FLAG;
    }
    return f;
}
static bool profile_nonce_valid(const og_frame_t *f) {
    return og_profile_control_valid(f);
}
static void output(const char *fmt, ...) {
    char line[540];va_list ap;va_start(ap,fmt);
    const int n=vsnprintf(line,sizeof(line),fmt,ap);va_end(ap);
    if(n<=0 || (size_t)n>=sizeof(line))return;
    /* USB and BLE receive identical protocol records. */
    fwrite(line,1,(size_t)n,stdout);
    offgrid_ble_publish(line);
}
static void hex_record(const char *prefix, const uint8_t *data, size_t n) {
    static const char hx[]="0123456789ABCDEF";
    char line[520];const size_t start=strlen(prefix);
    if(start+n*2+2>sizeof(line))return;
    memcpy(line,prefix,start);
    for(size_t i=0;i<n;i++){line[start+2*i]=hx[data[i]>>4];line[start+2*i+1]=hx[data[i]&15];}
    line[start+2*n]='\n';line[start+2*n+1]=0;
    fwrite(line,1,start+2*n+1,stdout);offgrid_ble_publish(line);
}
static bool parse_hex(const char *s,uint8_t *out,uint16_t *len) {
    const size_t size=strlen(s);if(!size || size>OG_MAX_DATA*2 || size%2)return false;
    for(size_t i=0;i<size/2;i++){
        char c=s[2*i],d=s[2*i+1];
        int h=isdigit((unsigned char)c)?c-'0':toupper((unsigned char)c)-'A'+10;
        int l=isdigit((unsigned char)d)?d-'0':toupper((unsigned char)d)-'A'+10;
        if(h<0||h>15||l<0||l>15)return false;
        out[i]=(uint8_t)((h<<4)|l);
    }
    *len=(uint16_t)(size/2);return true;
}
static bool nvs_key_load(void) {
    nvs_handle_t n;
    if(nvs_open("offgrid",NVS_READONLY,&n)!=ESP_OK)return false;
    size_t sz=sizeof(pair_key);
    esp_err_t e=nvs_get_blob(n,"pairkey",pair_key,&sz);
    nvs_close(n);
    return e==ESP_OK && sz==32;
}
static bool nvs_key_save(const uint8_t k[32]) {
    nvs_handle_t n;
    if(nvs_open("offgrid",NVS_READWRITE,&n)!=ESP_OK)return false;
    esp_err_t e=nvs_set_blob(n,"pairkey",k,32);
    if(e==ESP_OK)e=nvs_commit(n);
    nvs_close(n);
    if(e!=ESP_OK)return false;
    memcpy(pair_key,k,32);key_present=true;
    offgrid_ble_set_key(pair_key,true);
    return true;
}
static void print_key(void) {
    if(!key_present){puts("KEYEMPTY");return;}
    char line[74];memcpy(line,"KEYDATA|",8);
    const char *hx="0123456789ABCDEF";
    for(int i=0;i<32;i++){line[8+i*2]=hx[pair_key[i]>>4];line[9+i*2]=hx[pair_key[i]&15];}
    line[72]='\n';line[73]=0;
    /* KEYGET is allowed on physical USB ONLY, never forwarded over BLE. */
    fwrite(line,1,73,stdout);
}

/* Crypto domain separation: AES-GCM OGM4 key is NOT used directly as
 * radio HMAC key. All frames including HELLO/ACK have authenticated tags. */
static bool hmac(const uint8_t key[32], const uint8_t *msg,size_t size,uint8_t out[32]) {
    psa_key_attributes_t a=PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&a,PSA_KEY_TYPE_HMAC);
    psa_set_key_bits(&a,256);
    psa_set_key_usage_flags(&a,PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&a,PSA_ALG_HMAC(PSA_ALG_SHA_256));
    psa_key_id_t id=0;
    psa_status_t st=psa_import_key(&a,key,32,&id);
    psa_reset_key_attributes(&a);
    if(st!=PSA_SUCCESS)return false;
    size_t written=0;
    st=psa_mac_compute(id,PSA_ALG_HMAC(PSA_ALG_SHA_256),msg,size,out,32,&written);
    psa_status_t destroyed=psa_destroy_key(id);
    return st==PSA_SUCCESS && destroyed==PSA_SUCCESS && written==32;
}
static bool radio_hmac(const uint8_t *bytes, size_t n, uint8_t output[32]) {
    if (!radio_key_ready || !radio_psa_id) return false;
    size_t written=0;
    return psa_mac_compute(radio_psa_id,PSA_ALG_HMAC(PSA_ALG_SHA_256),
                           bytes,n,output,32,&written)==PSA_SUCCESS && written==32;
}
/* Bridge callbacks: use the same domain-separated radio key as OGM2,
 * with mandatory HMAC verification BEFORE feeding selective-repeat state. */
static bool arq_sign(void *ctx,const uint8_t *data,size_t n,uint8_t out16[16]){
    (void)ctx;uint8_t full[32];
    const bool ok=radio_hmac(data,n,full);
    if(ok)memcpy(out16,full,16);
    memset(full,0,sizeof(full));return ok;
}
static bool arq_verify(void *ctx,const uint8_t *data,size_t n,const uint8_t tag[16]){
    uint8_t actual[16]={0};
    if(!arq_sign(ctx,data,n,actual))return false;
    uint8_t diff=0;for(unsigned i=0;i<16;i++)diff|=actual[i]^tag[i];
    return diff==0;
}
static bool arq_send_radio(void *ctx,const uint8_t *wire,size_t n){
    (void)ctx;
    if(!radio_key_ready||!sx||n>OFFGRID_SX1278_MAX_PAYLOAD)return false;
    const uint32_t limit=profile_state.current==OG_MODE_ULTRA ?
        og_ultra_airtime_ms(12,125000,OG_ULTRA_CR_DENOM,8,(uint16_t)n)+1500u :
        OG_RADIO_TX_TIMEOUT_MS;
    const esp_err_t e=offgrid_sx1278_send(sx,wire,n,limit);
    if(e==ESP_OK){stat_tx++;arq_tx_done++;return true;}
    stat_tx_errors++;return false;
}
/* One line per USB event; no oversized BLE notifications for bulk data. */
static void arq_emit_usb(void *ctx,const char *line){
    (void)ctx;
    if(!line)return;
    fwrite(line,1,strlen(line),stdout);
    fputc('\n',stdout);
    /* USB remains the original v1.8 transport. Authenticated LoRa BLE clients
     * also need the exact same bulk status and RXBCHUNK ciphertext records. */
    offgrid_ble_publish(line);
}
static uint32_t arq_now_ms(void *ctx){(void)ctx;return (uint32_t)millis();}
static const og_arq_io_t arq_io={.context=NULL,.radio_tx=arq_send_radio,
    .serial_line=arq_emit_usb,.now_ms=arq_now_ms,
    .sign=arq_sign,.verify=arq_verify,.crypto=NULL};
static bool bulk_hex_id(const char *id,uint32_t *session,uint32_t *transfer){
    if(!id || strlen(id)!=16 || !session || !transfer)return false;
    uint8_t b[8];
    for(unsigned i=0;i<8;i++){
        char a=id[2*i],z=id[2*i+1];
        int hi=isdigit((unsigned char)a)?a-'0':toupper((unsigned char)a)-'A'+10;
        int lo=isdigit((unsigned char)z)?z-'0':toupper((unsigned char)z)-'A'+10;
        if(hi<0||hi>15||lo<0||lo>15)return false;
        b[i]=(uint8_t)((hi<<4)|lo);
    }
    uint32_t first=0,second=0;
    for(unsigned i=0;i<4;i++){
        first|=(uint32_t)b[i]<<(8*i);
        second|=(uint32_t)b[i+4]<<(8*i);
    }
    if(!first||!second)return false;
    *session=first;*transfer=second;return true;
}
static void derive_radio_key(const uint8_t k[32]) {
    uint8_t h[32]={0};
    if(radio_psa_id){(void)psa_destroy_key(radio_psa_id);radio_psa_id=0;}
    radio_key_ready=hmac(k,KDF_CONTEXT,sizeof(KDF_CONTEXT)-1,h);
    if(radio_key_ready){
        memcpy(radio_key,h,32);
        psa_key_attributes_t a=PSA_KEY_ATTRIBUTES_INIT;
        psa_set_key_type(&a,PSA_KEY_TYPE_HMAC);
        psa_set_key_bits(&a,256);
        psa_set_key_usage_flags(&a,PSA_KEY_USAGE_SIGN_MESSAGE);
        psa_set_key_algorithm(&a,PSA_ALG_HMAC(PSA_ALG_SHA_256));
        radio_key_ready=psa_import_key(&a,h,32,&radio_psa_id)==PSA_SUCCESS;
        psa_reset_key_attributes(&a);
    }
    if(!radio_key_ready){radio_psa_id=0;memset(radio_key,0,32);}
    memset(h,0,32);
    /* The old peer is not trusted after a key rotation. */
    peer_seen=false;peer_arq_seen=false;memset(peer_mac,0,6);
    memset(pending,0,sizeof(pending));memset(received,0,sizeof(received));
    memset(controls,0,sizeof(controls));
    profile_state.phase=OG_PROFILE_IDLE;
    profile_assist_until_ms=0;profile_assist_next_ms=0;
    last_profile_offer_valid=false;
    auto_enabled=auto_ctrl_pending=auto_peer_enabled=false;
    auto_seen_control=false;auto_last_accepted_id=auto_last_accepted_session=0;
    auto_last_metric_id=auto_last_metric_session=0;
    auto_last_metric_loss_count=0;auto_peer_recent_losses=0;
    og_auto_reset(&auto_policy,(uint32_t)millis());
    og_recovery_reset(&recovery);
    og_arq_bridge_cancel(&arq,&arq_io,"PAIRING_KEY_CHANGED");
}
static bool equal_tag(const uint8_t *a,const uint8_t *b) {
    uint8_t mismatch=0;
    for(size_t i=0;i<OG_AUTH_BYTES;i++)mismatch|=a[i]^b[i];
    return mismatch==0;
}

static og_frame_t new_frame(uint8_t type, uint32_t session,uint32_t id) {
    og_frame_t f={0};f.magic=OG_MAGIC;f.version=OG_VERSION;f.type=type;
    memcpy(f.sender,own_mac,6);f.session=session;f.id=id;
    return f;
}
/* Radio ACKs are not allowed to disappear behind an old discovery HELLO.
 * Dedupe repeated ACKs for the same frame. This queue is owned by radio_loop.
 */
static bool control_add(const og_frame_t *f,uint32_t delay_ms) {
    /* Low-rate diagnostic metrics are replaceable. An RF backlog must not
     * accumulate old measurements and starve PROFILE/ACK controls. */
    if(f->type==OG_TYPE_AUTO_METRIC){
        for(size_t i=0;i<OG_CTRL_QUEUE;i++)if(
            controls[i].frame.magic && controls[i].frame.type==OG_TYPE_AUTO_METRIC){
            controls[i].frame=*f;controls[i].due_ms=millis()+delay_ms;
            return true;
        }
    }
    if(f->type==OG_TYPE_RADIO_ACK || f->type==OG_TYPE_PROFILE_AGREE ||
       f->type==OG_TYPE_PROFILE_REJECT || f->type==OG_TYPE_AUTO_ACK ||
       f->type==OG_TYPE_AUTO_CTRL){
        for(size_t i=0;i<OG_CTRL_QUEUE;i++)if(
            controls[i].frame.magic && controls[i].frame.type==f->type &&
            controls[i].frame.session==f->session && controls[i].frame.id==f->id){
            int64_t target=millis()+delay_ms;
            if(target<controls[i].due_ms)controls[i].due_ms=target;
            return true;
        }
    }
    for(size_t i=0;i<OG_CTRL_QUEUE;i++)if(!controls[i].frame.magic){
        controls[i].frame=*f;controls[i].due_ms=millis()+delay_ms;return true;
    }
    /* Best-effort HELLO traffic must never displace a real radio ACK. */
    if(f->type==OG_TYPE_RADIO_ACK || f->type==OG_TYPE_PROFILE_AGREE ||
       f->type==OG_TYPE_PROFILE_REJECT || f->type==OG_TYPE_AUTO_ACK ||
       f->type==OG_TYPE_AUTO_CTRL){
        for(size_t i=0;i<OG_CTRL_QUEUE;i++)if(controls[i].frame.type==OG_TYPE_HELLO ||
                                              controls[i].frame.type==OG_TYPE_ARQ_CAPS){
            controls[i].frame=*f;controls[i].due_ms=millis()+delay_ms;return true;
        }
    }
    output("ERROR|CONTROL_QUEUE_FULL|TYPE=%u\n",(unsigned)f->type);return false;
}
static esp_err_t tx(const og_frame_t *f) {
    if(!radio_key_ready)return ESP_ERR_INVALID_STATE;
    const size_t n=og_wire_length(f);
    uint8_t bytes[OG_AIR_MAX], digest[32];
    memcpy(bytes,f,n);
    if(!radio_hmac(bytes,n,digest))return ESP_FAIL;
    memcpy(bytes+n,digest,OG_AUTH_BYTES);memset(digest,0,sizeof(digest));
    const uint32_t limit=profile_state.current==OG_MODE_ULTRA ?
        og_ultra_airtime_ms(12,125000,OG_ULTRA_CR_DENOM,8,(uint16_t)(n+OG_AUTH_BYTES))+1500u :
        OG_RADIO_TX_TIMEOUT_MS;
    esp_err_t e=offgrid_sx1278_send(sx,bytes,n+OG_AUTH_BYTES,limit);
    if(e==ESP_OK)stat_tx++;
    else stat_tx_errors++;
    if(e!=ESP_OK)output("RADIO_DIAG|TX_ERROR|%s\n",esp_err_to_name(e));
    return e;
}
static void profile_report(const char *state) {
    offgrid_sx1278_profile_t p=og_mode_profile(profile_state.current);
    if(auto_enabled && profile_state.current==OG_MODE_BALANCED){p.tx_power_dbm=18;p.high_pa_dac=true;}
    output("PROFILE|%s|%s|SF=%u|BW=%" PRIu32 "|TX_DBM=%d|AUTO=%u\n",
           state,og_mode_name(profile_state.current),(unsigned)p.spreading_factor,
           (uint32_t)p.bandwidth,(int)p.tx_power_dbm,(unsigned)auto_enabled);
}
static void profile_reset_pending(void) {
    og_arq_bridge_cancel(&arq,&arq_io,"PROFILE_CHANGED_UNCONFIRMED");
    /* Key or mode changes never fake delivered/read receipts. */
    for(size_t i=0;i<OG_PENDING;i++)if(pending[i].used){
        output("FAILED|%" PRIu32 "|%" PRIu32 "|PROFILE_CHANGED_UNCONFIRMED\n",
               pending[i].frame.session,pending[i].frame.id);
        pending[i].used=false;stat_fail++;
    }
    memset(controls,0,sizeof(controls));
}
static void profile_start_offer(uint8_t target, bool force, bool automatic, bool media) {
    if(recovery.probing){output("PROFILE|ERROR|LINK_RECOVERY_IN_PROGRESS\n");return;}
    if(!radio_key_ready||!sx){output("PROFILE|ERROR|RADIO_NOT_READY\n");return;}
    if(!og_profile_mode_valid(target)){output("PROFILE|ERROR|INVALID_MODE\n");return;}
    if(og_profile_high_pa(target)&&!high_pa_verified){
        output("PROFILE|ERROR|18_DBM_REQUIRES_LOCAL_POWER_VERIFICATION\n");return;
    }
    if(profile_state.phase!=OG_PROFILE_IDLE){output("PROFILE|ERROR|TRANSITION_IN_PROGRESS\n");return;}
    if(target==OG_MODE_ULTRA && !peer_ultra_capable){
        output("PROFILE|ERROR|PEER_REQUIRES_V305_ULTRA_FEC8\n");return;
    }
    if(target==profile_state.current){profile_report("ACTIVE");return;}
    if(profile_has_pending()&&!force){output("PROFILE|ERROR|PENDING_MESSAGES_USE_FORCE\n");return;}
    if(!peer_seen||millis()-peer_at_ms>OG_PEER_STALE_MS){output("PROFILE|ERROR|NO_AUTHENTICATED_PEER\n");return;}
    if(force)profile_reset_pending();
    if(!automatic && !media){auto_enabled=false;auto_peer_enabled=false;auto_ctrl_pending=false;}
    auto_requested_profile=automatic;
    media_requested_profile=media;
    for(size_t i=0;i<OG_CTRL_QUEUE;i++)if(controls[i].frame.type==OG_TYPE_AUTO_METRIC)
        controls[i].frame.magic=0;
    profile_verify_initiator=true;
    profile_state.previous=profile_state.current;
    profile_state.target=target;
    profile_state.phase=OG_PROFILE_OFFERING;
    profile_assist_until_ms=0;profile_assist_next_ms=0;
    profile_state.nonce=esp_random();if(!profile_state.nonce)profile_state.nonce=1;
    profile_state.offer_session=boot_session;
    profile_state.attempts=0;
    profile_state.retry_at_ms=0;
    output("PROFILE|NEGOTIATING|%s|MANUAL=%u\n",og_mode_name(target),(unsigned)!automatic);
}
static bool profile_apply(uint8_t mode) {
    offgrid_sx1278_profile_t p=og_mode_profile(mode);
    if(auto_enabled && mode==OG_MODE_BALANCED){p.tx_power_dbm=18;p.high_pa_dac=true;}
    esp_err_t e=offgrid_sx1278_set_profile(sx,&p);
    if(e!=ESP_OK){output("PROFILE|ERROR|RADIO_CONFIG_%s\n",esp_err_to_name(e));return false;}
    profile_state.current=mode;
    og_arq_bridge_set_ultra(&arq,mode==OG_MODE_ULTRA,48u,1u);
    og_auto_reset(&auto_policy,(uint32_t)millis());
    auto_defer_until_ms=millis()+OG_AUTO_COOLDOWN_MS;
    /* New data can only flow once an authenticated HELLO proves both peers
     * really switched. Until then, hold normal outgoing transmissions. */
    return true;
}
/* On a prolonged authenticated-peer silence, temporarily rendezvous on
 * Balanced (the safe, common 10 dBm boot modulation). Profile probing never
 * makes another node appear 'delivered' and never accepts unauthenticated
 * data. The node goes back home if nobody is heard during the scan. */
static void link_recovery_service(void){
    const uint32_t now=(uint32_t)millis();
    if(!radio_key_ready || !sx || !peer_seen ||
       profile_state.phase!=OG_PROFILE_IDLE || auto_ctrl_pending)return;
    const uint32_t age=(uint32_t)(millis()-peer_at_ms);
    if(media_boost_active && age>OG_RECOVERY_ULTRA_LOSS_MS){
        /* An abandoned media Fast boost must not prevent rediscovery for
         * ten minutes after the phone or far-end radio loses contact. */
        media_boost_active=false;media_restore_requested=false;
        media_active_until_ms=0;
        output("MEDIA|ERROR|LINK_LOST_RELEASE_BOOST_FOR_RECOVERY\n");
    }
    if(media_boost_active)return;
    /* If both nodes are already in the common profile yet authenticated
     * traffic has been silent for >2 minutes, restart continuous RX once.
     * Guard against repeatedly resetting the modem during an RF packet. */
    if(!recovery.probing && profile_state.current==OG_MODE_BALANCED &&
       age>120000u && !profile_has_pending() &&
       (millis()-last_recovery_rx_refresh_ms)>120000){
        last_recovery_rx_refresh_ms=millis();
        offgrid_sx1278_profile_t stable=og_mode_profile(OG_MODE_BALANCED);
        if(auto_enabled && high_pa_verified){
            stable.tx_power_dbm=18;stable.high_pa_dac=true;
        }
        if(offgrid_sx1278_set_profile(sx,&stable)==ESP_OK){
            last_hello_ms=-OG_DISCOVERY_HELLO_MS;
            output("RECOVERY|RX_REARM|BALANCED|NO_AUTHENTICATED_PEER\n");
        }else output("RECOVERY|ERROR|RX_REARM_FAILED\n");
    }
    const bool blocked=(age<OG_PEER_STALE_MS) ||
        (profile_state.current==OG_MODE_BALANCED && !recovery.probing);
    const og_recovery_action_t action=og_recovery_step(&recovery,now,age,
           profile_state.current,blocked,false);
    if(action==OG_RECOVERY_ENTER_BALANCED){
        /* These are ciphertext packets; the Android/web outbox retains them
         * by message ID for later retry after peer discovery. */
        if(profile_has_pending())profile_reset_pending();
        if(!profile_apply(OG_MODE_BALANCED)){
            og_recovery_reset(&recovery);return;
        }
        recovery_scans++;
        last_hello_ms=-OG_DISCOVERY_HELLO_MS;
        memset(controls,0,sizeof(controls));
        output("RECOVERY|SCANNING|BALANCED|HOME=%s|WINDOW_MS=%u\n",
               og_mode_name(recovery.home),(unsigned)OG_RECOVERY_SCAN_MS);
        profile_report("RECOVERY_SCAN");
    }else if(action==OG_RECOVERY_RETURN_HOME){
        const uint8_t home=recovery.home;
        if(!profile_apply(home)){
            /* Keep listening on the safe common profile if the old
             * modulation fails to apply: do NOT pretend it returned. */
            og_recovery_reset(&recovery);
            output("RECOVERY|ERROR|HOME_PROFILE_UNAVAILABLE\n");
            return;
        }
        memset(controls,0,sizeof(controls));
        last_hello_ms=-OG_DISCOVERY_HELLO_MS;
        output("RECOVERY|HOME|%s|NEXT_SCAN_MS=%" PRIu32 "\n",
               og_mode_name(home),recovery.next_scan_ms-now);
        profile_report("RECOVERY_HOME");
    }
}
static void profile_service(void) {
    const int64_t now=millis();
    if(profile_state.phase==OG_PROFILE_RESPONDING && now>=profile_state.retry_at_ms){
        for(size_t i=0;i<OG_CTRL_QUEUE;i++)if(
            controls[i].frame.magic && controls[i].frame.type==OG_TYPE_PROFILE_AGREE &&
            controls[i].frame.id==profile_state.nonce &&
            controls[i].frame.session==profile_state.offer_session)
                controls[i].frame.magic=0;
        profile_state.phase=OG_PROFILE_IDLE;
        output("PROFILE|ERROR|AGREE_RESPONSE_TIMEOUT\n");
    }
    if(profile_state.phase==OG_PROFILE_OFFERING && now>=profile_state.retry_at_ms){
        if(profile_state.attempts>=5){
            profile_state.phase=OG_PROFILE_IDLE;
            auto_defer_until_ms=millis()+60000;
            auto_failures++;
            output("PROFILE|TIMEOUT|%s|PEER_DID_NOT_AGREE\n",og_mode_name(profile_state.target));
            if(media_requested_profile){
                if(profile_state.target==OG_MODE_LOW){
                    media_boost_active=false;
                    output("MEDIA|BYPASS|BOOST_HANDSHAKE_TIMEOUT\n");
                }else output("MEDIA|RESTORE_RETRY|PEER_DID_NOT_AGREE\n");
            }
        }else{
            og_frame_t offer=profile_frame(OG_TYPE_PROFILE_OFFER,profile_state.target,
                                           profile_state.offer_session,profile_state.nonce);
            (void)tx(&offer);
            profile_state.attempts++;
            profile_state.retry_at_ms=millis()+(profile_state.current==OG_MODE_ULTRA?6500:OG_PROFILE_OFFER_RETRY_MS);
        }
    }
    if(profile_state.phase==OG_PROFILE_WAIT_SWITCH && now>=profile_state.switch_at_ms){
        const uint8_t old=profile_state.previous;
        if(!profile_apply(profile_state.target)){
            (void)profile_apply(old);
            profile_state.phase=OG_PROFILE_IDLE;
            auto_defer_until_ms=millis()+60000;
            auto_failures++;
            profile_report("ROLLED_BACK");
            if(media_requested_profile && profile_state.target==OG_MODE_LOW){
                media_boost_active=false;output("MEDIA|BYPASS|FAST_RADIO_CONFIG_FAILED\n");
            }
            return;
        }
        profile_state.phase=OG_PROFILE_VERIFYING;
        profile_state.verify_deadline_ms=millis()+
             (profile_state.current==OG_MODE_ULTRA?OG_ULTRA_VERIFY_MAX_MS:OG_PROFILE_VERIFY_MS);
        profile_state.probe_at_ms=millis()+og_verify_first_delay_ms(profile_state.current,
                                                        profile_verify_initiator);
        output("PROFILE|VERIFYING|%s|ROLE=%s|FIRST_HELLO_MS=%" PRIu32 "\n",
               og_mode_name(profile_state.current),
               profile_verify_initiator?"INITIATOR":"RESPONDER",
               og_verify_first_delay_ms(profile_state.current,profile_verify_initiator));
    }
    if(profile_state.phase==OG_PROFILE_VERIFYING){
        if(now>=profile_state.verify_deadline_ms){
            (void)profile_apply(profile_state.previous);
            profile_state.phase=OG_PROFILE_IDLE;stat_profile_rollbacks++;
            profile_assist_until_ms=0;profile_assist_next_ms=0;
            memset(controls,0,sizeof(controls));
            auto_defer_until_ms=millis()+60000;
            auto_failures++;
            profile_report("ROLLED_BACK");
            if(media_requested_profile && profile_state.target==OG_MODE_LOW){
                media_boost_active=false;output("MEDIA|BYPASS|FAST_VERIFY_TIMEOUT\n");
            }else if(media_requested_profile){
                output("MEDIA|RESTORE_RETRY|VERIFY_TIMEOUT\n");
            }
        }else if(now>=profile_state.probe_at_ms){
            /* Authenticated HELLOs let both nodes confirm the new RF profile.
             * No application traffic or benchmark traffic until verified. */
            og_frame_t hello=new_frame(OG_TYPE_HELLO,boot_session,0);
            (void)tx(&hello);
            profile_state.probe_at_ms=millis()+
                og_verify_repeat_after_tx_ms(profile_state.current)+
                (profile_state.current==OG_MODE_ULTRA?0:(esp_random()%250));
        }
    }
    if(profile_state.phase==OG_PROFILE_IDLE && profile_assist_until_ms>now &&
       now>=profile_assist_next_ms && !profile_has_pending()){
        og_frame_t hello=new_frame(OG_TYPE_HELLO,boot_session,0);
        (void)tx(&hello);
        profile_assist_next_ms=millis()+
            (profile_state.current==OG_MODE_ULTRA?OG_ULTRA_VERIFY_REPEAT_AFTER_TX_MS:530)+
            (profile_state.current==OG_MODE_ULTRA?0:(esp_random()%220));
    }
}
static og_frame_t auto_control_frame(uint8_t type,bool enable,uint32_t session,uint32_t nonce){
    og_frame_t f=new_frame(type,session,nonce);
    f.length=6;f.data[0]=enable?1u:0u;f.data[1]=enable?0xA5u:0u;
    for(unsigned i=0;i<4;i++)f.data[i+2]=(uint8_t)(nonce>>(i*8u));
    return f;
}
static void auto_report(const char *state){
    output("AUTO|%s|MODE=%s|ENABLED=%u|PEER=%u|LEADER=%u|LOCAL_RSSI=%d|PEER_RSSI=%d|LOCAL_SNR_Q4=%d|PEER_SNR_Q4=%d|SAMPLES=%u/%u|QUALITY_READY=%u|LOCAL_AGE_MS=%" PRId64 "|REMOTE_AGE_MS=%" PRId64 "\n",
        state,og_mode_name(profile_state.current),(unsigned)auto_enabled,
        (unsigned)auto_peer_enabled,(unsigned)(peer_seen&&memcmp(own_mac,peer_mac,6)<0),
        (int)auto_policy.local.rssi_dbm,(int)auto_policy.remote.rssi_dbm,
        (int)auto_policy.local.snr_q4,(int)auto_policy.remote.snr_q4,
        (unsigned)auto_policy.local.samples,(unsigned)auto_policy.remote.samples,
        (unsigned)(og_auto_fresh(&auto_policy.local,(uint32_t)millis()) &&
                   og_auto_fresh(&auto_policy.remote,(uint32_t)millis())),
        (int64_t)(millis()-auto_policy.local.at_ms),
        (int64_t)(millis()-auto_policy.remote.at_ms));
}
static void auto_command(bool enable){
    if(!radio_key_ready||!sx||!peer_seen||millis()-peer_at_ms>OG_PEER_STALE_MS){
        output("AUTO|ERROR|NO_AUTHENTICATED_PEER\n");return;
    }
    if(profile_state.phase!=OG_PROFILE_IDLE||auto_ctrl_pending||profile_has_pending()){
        output("AUTO|ERROR|BUSY_FINISH_TRANSFERS\n");return;
    }
    if(enable && !high_pa_verified){
        output("AUTO|ERROR|VERIFY_BOTH_BOARDS_POWER18_FIRST\n");return;
    }
    if(enable==auto_enabled){auto_report(enable?"ON":"OFF");return;}
    auto_ctrl_pending=true;auto_ctrl_target=enable;
    auto_ctrl_nonce=next_id++;if(!auto_ctrl_nonce)auto_ctrl_nonce=next_id++;
    auto_ctrl_session=boot_session;
    auto_ctrl_retries=0;auto_ctrl_retry_at=0;
    output("AUTO|NEGOTIATING|%s\n",enable?"ON":"OFF");
}
static void auto_service(void){
    int64_t now=millis();
    /* Media session expiry and restore are owned by media_service(). */
    if(auto_ctrl_pending && now>=auto_ctrl_retry_at &&
       profile_state.phase==OG_PROFILE_IDLE && !profile_has_pending()){
        if(auto_ctrl_retries>=6){
            auto_ctrl_pending=false;auto_failures++;
            output("AUTO|ERROR|PEER_DID_NOT_CONFIRM\n");
        }else{
            og_frame_t f=auto_control_frame(OG_TYPE_AUTO_CTRL,auto_ctrl_target,
                                             auto_ctrl_session,auto_ctrl_nonce);
            /* Direct from the sole RF owner, identical to the working
             * profile-offer retry path. No speculative RF CAD gate. */
            (void)tx(&f);
            auto_ctrl_retries++;
            auto_ctrl_retry_at=millis()+1050;
        }
    }
    if(!auto_enabled||recovery.probing||profile_state.phase!=OG_PROFILE_IDLE||!peer_seen||
       now-peer_at_ms>OG_PEER_STALE_MS)return;
    if(now-auto_last_metric_ms>=
       (profile_state.current==OG_MODE_ULTRA?20000:OG_AUTO_METRIC_PERIOD_MS) &&
       !auto_ctrl_pending){
        auto_last_metric_ms=now+(esp_random()%300)+(own_mac[5]%8)*85;
        og_frame_t m=new_frame(OG_TYPE_AUTO_METRIC,boot_session,next_id++);
        m.length=8;
        const int16_t r=auto_policy.local.rssi_dbm,s=auto_policy.local.snr_q4;
        m.data[0]=(uint8_t)r;m.data[1]=(uint8_t)((uint16_t)r>>8);
        m.data[2]=(uint8_t)s;m.data[3]=(uint8_t)((uint16_t)s>>8);
        m.data[4]=og_auto_fresh(&auto_policy.local,(uint32_t)now)?auto_policy.local.samples:0;
        m.data[5]=profile_state.current;
        m.data[6]=1;
        const uint32_t current_losses=stat_crc+arq.retransmits+arq.ack_timeouts;
        const uint32_t loss_delta=current_losses-auto_last_metric_loss_count;
        auto_last_metric_loss_count=current_losses;
        m.data[7]=(uint8_t)(loss_delta>15?15:loss_delta);
        (void)control_add(&m,50+(esp_random()%75));
    }
    /* Exactly ONE coordinator per peer pair may initiate auto switches.
     * At weak signal, bypass scheduling deferral while retaining signed
     * OFFER/AGREE/verify. Never switch only one node by RSSI alone. */
    /* Local authenticated RSSI is enough for an urgent DOWNGRADE proposal
     * when the peer's optional quality report was lost. The remote must
     * already have authenticated AUTO consent; peer must still sign AGREE. */
    const bool local_emergency=auto_peer_enabled &&
        og_auto_fresh(&auto_policy.local,(uint32_t)now) &&
        (uint32_t)(now-auto_policy.local.at_ms)<22000u &&
        ((profile_state.current==OG_MODE_LOW &&
          (auto_policy.local.rssi_dbm<=OG_AUTO_FAST_DOWN_RSSI ||
           auto_policy.local.snr_q4<=OG_AUTO_FAST_DOWN_SNR_Q4)) ||
         (profile_state.current==OG_MODE_BALANCED &&
          (auto_policy.local.rssi_dbm<=OG_AUTO_BAL_DOWN_RSSI ||
           auto_policy.local.snr_q4<=OG_AUTO_BAL_DOWN_SNR_Q4)) ||
         (profile_state.current==OG_MODE_MAX &&
          (auto_policy.local.rssi_dbm<=-108 ||
           auto_policy.local.snr_q4<=-16)));
    const bool emergency=local_emergency ||
        (og_auto_fresh(&auto_policy.local,(uint32_t)now) &&
        og_auto_fresh(&auto_policy.remote,(uint32_t)now) &&
        ((profile_state.current==OG_MODE_LOW &&
          (auto_policy.local.rssi_dbm<=OG_AUTO_FAST_DOWN_RSSI || auto_policy.remote.rssi_dbm<=OG_AUTO_FAST_DOWN_RSSI ||
           auto_policy.local.snr_q4<=OG_AUTO_FAST_DOWN_SNR_Q4 || auto_policy.remote.snr_q4<=OG_AUTO_FAST_DOWN_SNR_Q4)) ||
         (profile_state.current==OG_MODE_BALANCED &&
          (auto_policy.local.rssi_dbm<=OG_AUTO_BAL_DOWN_RSSI || auto_policy.remote.rssi_dbm<=OG_AUTO_BAL_DOWN_RSSI ||
           auto_policy.local.snr_q4<=OG_AUTO_BAL_DOWN_SNR_Q4 || auto_policy.remote.snr_q4<=OG_AUTO_BAL_DOWN_SNR_Q4)) ||
         (profile_state.current==OG_MODE_MAX &&
          (auto_policy.local.rssi_dbm<=-113 || auto_policy.remote.rssi_dbm<=-113 ||
           auto_policy.local.snr_q4<=-20 || auto_policy.remote.snr_q4<=-20))));
    if(memcmp(own_mac,peer_mac,6)>=0 || !auto_peer_enabled ||
       (media_boost_active && profile_state.current==OG_MODE_LOW && !emergency) ||
       (now<auto_defer_until_ms && !emergency) || auto_ctrl_pending)return;
    if((uint32_t)(now-auto_policy.last_decision_ms)<OG_AUTO_METRIC_PERIOD_MS)return;
    const uint32_t rf_retx=arq.retransmits+arq.ack_timeouts;
    bool lossy=auto_peer_recent_losses>=2;
    if(auto_policy.sample_counters_ready){
        const uint32_t diff=rf_retx-auto_policy.last_local_retx;
        lossy=lossy || diff>=2u;
    }
    auto_policy.last_local_retx=rf_retx;
    auto_policy.sample_counters_ready=true;
    auto_peer_recent_losses=0; /* do not reuse a 3.5-second loss report */
    const uint8_t chosen=og_auto_decide_linkaware(&auto_policy,profile_state.current,
                                   (uint32_t)now,false,lossy,auto_peer_enabled);
    if(chosen!=profile_state.current){
        if(chosen==OG_MODE_ULTRA && !peer_ultra_capable){
            output("AUTO|SKIP|ULTRA_REQUIRES_BOTH_V305_FEC8\n");return;
        }
        /* A stalled weak-link transfer must not permanently block a safety
         * downgrade. Fail it explicitly so the encrypted app outbox retries
         * after the authenticated new-mode handshake. Never fake a receipt. */
        if(profile_has_pending()){
            if(!emergency && !lossy){
                if(now-auto_last_block_report_ms>10000){
                    auto_last_block_report_ms=now;
                    output("AUTO|WAIT|PENDING_TRANSFER\n");
                }
                return;
            }
            output("AUTO|PREEMPT|WEAK_LINK_ARQ|REQUEUE_IN_CLIENT\n");
            profile_reset_pending();
        }
        auto_attempts++;
        output("AUTO|SELECT|%s|REASON=RSSI_SNR_LOSS\n",og_mode_name(chosen));
        profile_start_offer(chosen,false,true,false);
    }
}
/* Media hint expiration and restoration run only on the RF owner task.
 * Authenticated OFFER/AGREE and HELLO verify the return on BOTH radios.
 * Busy handling never calls profile_reset_pending(). */
static void media_service(void){
    const int64_t now=millis();
    if(media_boost_active && media_active_until_ms && now>=media_active_until_ms){
        media_active_until_ms=0;
        if(profile_state.current!=media_original_mode){
            /* A crashed client cannot send MEDIA OFF. Only the elected low-MAC
             * peer initiates expired-session recovery, avoiding crossed offers. */
            if(peer_seen && memcmp(own_mac,peer_mac,6)<0)media_restore_requested=true;
        }else {media_boost_active=false;media_restore_requested=false;}
    }
    if(!media_restore_requested || !media_boost_active ||
       profile_state.phase!=OG_PROFILE_IDLE || profile_has_pending() ||
       now<media_restore_retry_ms)return;
    if(profile_state.current==media_original_mode){
        media_boost_active=false;media_restore_requested=false;
        output("MEDIA|RESTORED|MODE=%s\n",og_mode_name(profile_state.current));return;
    }
    if(!peer_seen || now-peer_at_ms>OG_PEER_STALE_MS){
        media_restore_retry_ms=now+2200;
        output("MEDIA|RESTORE_WAIT|NO_AUTHENTICATED_PEER\n");return;
    }
    if(media_restore_attempts>=3){
        media_restore_requested=false;
        output("MEDIA|RESTORE_FAILED|MANUAL_RECOVERY_REQUIRED\n");return;
    }
    media_restore_attempts++;
    media_restore_retry_ms=now+3000;
    profile_start_offer(media_original_mode,false,false,true);
}
static void show_ready(const char *source) {
    char mac[18];mac_text(own_mac,mac);
    output("READY|%s|CHANNEL=0|SESSION=%" PRIu32 "|PROTO=2|CAP=180|STABLE=1|NVSKEY=1|FAMILY=LORA|RADIO=SX1278|FW=1.8.0-rc.4|DIAG=1|SACK=0|ACKENGINE=2|FLOW=2|PROFILES=4|SYNC=ONE_CLICK|AUTORF=1|ARQ=1|BULK=1|FRAG=224|WINDOW=6|USBPIPE=4|MEDIA=1|MEDIAFAST=1|RF_POLICY=ULTRA12_FEC8|RF_LINK_AGE=1|ULTRA=SF12_TEXT_ONLY|PROFILE_SYNC=ULTRA_SLOTS4|LINK_RECOVERY=RENDEZVOUS36|AUTO_FALLBACK=LOCAL_AUTH|BENCH=APP|FROM=%s\n",mac,boot_session,source);
}
static void show_link(void) {
    output("RECOVERY|%s|HOME=%s|SCANS=%" PRIu32 "|LOCKS=%" PRIu32
           "|AUTO_LOCAL=%u|AUTO_REMOTE=%u\n",
           recovery.probing?"SCAN_BALANCED":"LISTEN",
           og_mode_name(recovery.probing?recovery.home:profile_state.current),
           recovery_scans,recovery_locks,
           (unsigned)og_auto_fresh(&auto_policy.local,(uint32_t)millis()),
           (unsigned)og_auto_fresh(&auto_policy.remote,(uint32_t)millis()));
    char p[18];if(peer_seen)mac_text(peer_mac,p);else strcpy(p,"NONE");
    unsigned pending_count=0,ctrl_count=0;
    for(size_t i=0;i<OG_PENDING;i++)if(pending[i].used)pending_count++;
    for(size_t i=0;i<OG_CTRL_QUEUE;i++)if(controls[i].frame.magic)ctrl_count++;
    const int64_t age_ms=peer_seen ? (millis()-peer_at_ms) : -1;
    output("LINK|%s|RSSI=%d|PEER_AGE_MS=%" PRId64 "|RFHEARD=%u|PHY=0|MODE=LORA|OK=%" PRIu32 "|RETRY=%" PRIu32
           "|FAIL=%" PRIu32 "|TXOK=%" PRIu32 "|TXFAIL=%" PRIu32 "|RX=%" PRIu32
           "|HELLO=%" PRIu32 "|CHANGES=0|SNR_Q4=%d|RADIOACK=%" PRIu32
           "|BADTAG=%" PRIu32 "|CRC=%" PRIu32 "|TXDONE=%" PRIu32
           "|APPACK=%" PRIu32 "|ARQ_TX=%" PRIu32 "|ARQ_ACKTX=%" PRIu32 "|ARQ_ACKRX=%" PRIu32 "|ARQ_RETX=%" PRIu32 "|ARQ_TIMEOUT=%" PRIu32 "|ARQ_BADMAC=%" PRIu32 "|ARQ_RXDUP=%" PRIu32 "|ARQ_USBERR=%" PRIu32 "|CAD_BUSY=%" PRIu32 "|PENDING=%u|CTRLQ=%u|PEER_ARQ=%u|PROFILE=%s|PROFILE_PHASE=%d|PROFILE_CHANGES=%" PRIu32 "|PROFILE_ROLLBACKS=%" PRIu32 "|AUTO=%u|AUTO_PEER=%u|AUTO_ATTEMPTS=%" PRIu32 "|AUTO_FAILURES=%" PRIu32 "\n",
           p,(int)peer_rssi,age_ms,(unsigned)(peer_seen && age_ms>=0 && age_ms<OG_PEER_STALE_MS),
           stat_delivered,stat_retry,stat_fail,stat_tx,stat_tx_errors,
           stat_rx,stat_hello,(int)auto_policy.local.snr_q4,stat_radio_ack,stat_badtag,stat_crc,
           stat_tx_done,stat_appack,arq.tx_frames,arq.bitmap_acks_tx,
           arq.bitmap_acks_rx,arq.retransmits,arq.ack_timeouts,
           arq.reject_auth,arq.rx_duplicates,arq.usb_errors,stat_busy_cad,
           pending_count,ctrl_count,(unsigned)peer_arq_seen,
           og_mode_name(profile_state.current),
           (int)profile_state.phase,stat_profile_changes,stat_profile_rollbacks,
           (unsigned)auto_enabled,(unsigned)auto_peer_enabled,auto_attempts,auto_failures);
}
static og_received_t *history_find(const uint8_t mac[6],uint32_t session,uint32_t id) {
    for(size_t i=0;i<OG_RX_HISTORY;i++)if(received[i].used &&
        received[i].session==session&&received[i].id==id&&
        !memcmp(received[i].sender,mac,6))return &received[i];
    return NULL;
}
static void incoming_browser(const og_frame_t *f) {
    char peer[18],prefix[100];mac_text(f->sender,peer);
    snprintf(prefix,sizeof(prefix),"RX|%s|%" PRIu32 "|%" PRIu32 "|",peer,f->session,f->id);
    hex_record(prefix,f->data,f->length);
}
static bool peer_accept(const og_frame_t *f,int16_t rssi) {
    if(!memcmp(f->sender,own_mac,6))return false;
    const int64_t t=millis();
    if(peer_seen && memcmp(peer_mac,f->sender,6)!=0 && t-peer_at_ms<OG_PEER_STALE_MS)return false;
    if(memcmp(peer_mac,f->sender,6)!=0){
        peer_ultra_capable=false;
        peer_arq_seen=false;auto_enabled=false;auto_peer_enabled=false;
        auto_ctrl_pending=false;og_auto_reset(&auto_policy,(uint32_t)millis());
    }
    memcpy(peer_mac,f->sender,6);peer_seen=true;peer_at_ms=t;peer_rssi=rssi;
    return true;
}
static void received_frame(const uint8_t *bytes,size_t n,int16_t rssi,int16_t snr_q4) {
    if(!radio_key_ready || n<OG_FRAME_HDR_BYTES+OG_AUTH_BYTES || n>OG_AIR_MAX)return;
    const size_t plain=n-OG_AUTH_BYTES;
    og_frame_t f={0};memcpy(&f,bytes,plain);
    if(!og_wire_valid(&f,plain))return;
    uint8_t digest[32];
    if(!radio_hmac(bytes,plain,digest) || !equal_tag(digest,bytes+plain)){
        stat_badtag++;memset(digest,0,sizeof(digest));return;
    }
    memset(digest,0,sizeof(digest));
    if(!peer_accept(&f,rssi))return;
    if(recovery.probing){
        (void)og_recovery_step(&recovery,(uint32_t)millis(),0,
              profile_state.current,false,true);
        recovery_locks++;
        peer_arq_seen=false;peer_ultra_capable=false;last_arq_caps_ms=0;
        auto_peer_enabled=false;auto_last_metric_session=0;auto_last_metric_id=0;
        auto_defer_until_ms=millis()+OG_RECOVERY_AUTO_DEFER_MS;
        output("RECOVERY|AUTHENTICATED|BALANCED|PEER_RESTORED\n");
        profile_report("ACTIVE");
    }
    og_auto_quality_note(&auto_policy.local,rssi,snr_q4,(uint32_t)millis());
    stat_rx++;
    /* A correct HMAC on the NEW profile is the final confirmation. */
    if(profile_state.phase==OG_PROFILE_VERIFYING){
        profile_state.phase=OG_PROFILE_IDLE;
        stat_profile_changes++;
        profile_assist_until_ms=millis()+
           (profile_state.current==OG_MODE_ULTRA?20000:3200);
        profile_assist_next_ms=millis()+
            og_verify_assist_after_rx_ms(profile_state.current,profile_verify_initiator)+
            (profile_state.current==OG_MODE_ULTRA?0:(esp_random()%180));
        profile_report("ACTIVE");
        if(media_requested_profile && media_boost_active){
            if(profile_state.current==OG_MODE_LOW){
                output("MEDIA|READY|MODE=LOW\n");
            }else if(profile_state.current==media_original_mode){
                media_boost_active=false;media_restore_requested=false;
                media_restore_attempts=0;media_active_until_ms=0;
                output("MEDIA|RESTORED|MODE=%s\n",og_mode_name(profile_state.current));
            }
        }
        if(auto_enabled)auto_report("ACTIVE");
    }
    if(f.type==OG_TYPE_HELLO){stat_hello++;return;}
    if(f.type==OG_TYPE_AUTO_CTRL){
        if(profile_state.phase!=OG_PROFILE_IDLE || profile_has_pending())return;
        const bool requested=f.data[0]!=0;
        /* Authentication and wire-length validation were already completed.
         * Track increasing IDs within the sender's boot session to reject
         * replays of older settings. Duplicate latest command is ACKed. */
        if(auto_seen_control && f.session==auto_last_accepted_session &&
           f.id<auto_last_accepted_id)return;
        if(!auto_seen_control || f.session!=auto_last_accepted_session ||
           f.id!=auto_last_accepted_id){
            auto_seen_control=true;auto_last_accepted_session=f.session;
            auto_last_accepted_id=f.id;
            const bool before=auto_enabled;
            auto_enabled=requested;
            if(requested)high_pa_verified=true; /* authenticated operator approval for both */
            if(profile_state.current==OG_MODE_BALANCED &&
               !profile_apply(OG_MODE_BALANCED)){
                auto_enabled=before;
                auto_seen_control=false; /* let peer retry this exact nonce */
                output("AUTO|ERROR|RF_CONFIG_FAILED\n");return;
            }
            auto_ctrl_pending=false;
            auto_last_metric_ms=millis()+(own_mac[5]%8)*85;
            auto_peer_enabled=requested;
            og_auto_reset(&auto_policy,(uint32_t)millis());
            auto_defer_until_ms=millis()+10000;
            if(requested){
                /* Prefer Fast once enough stable two-way samples arrive. */
                auto_policy.changed_ms=(uint32_t)millis()-OG_AUTO_COOLDOWN_MS;
                auto_policy.started_ms=(uint32_t)millis()-OG_AUTO_PROMOTION_MS;
                auto_defer_until_ms=millis()+11000;
            }
            profile_report("ACTIVE");
            auto_report(requested?"PEER_ON":"PEER_OFF");
        }
        og_frame_t reply=auto_control_frame(OG_TYPE_AUTO_ACK,requested,f.session,f.id);
        (void)control_add(&reply,30+(esp_random()%35));
        return;
    }
    if(f.type==OG_TYPE_AUTO_ACK){
        if(auto_ctrl_pending && f.session==auto_ctrl_session &&
           f.id==auto_ctrl_nonce && f.data[0]==(uint8_t)auto_ctrl_target){
            auto_ctrl_pending=false;
            const bool before=auto_enabled;
            auto_enabled=auto_ctrl_target;
            if(profile_state.current==OG_MODE_BALANCED &&
               !profile_apply(OG_MODE_BALANCED)){
                auto_enabled=before;auto_failures++;
                output("AUTO|ERROR|LOCAL_RF_CONFIG_FAILED_RESET_BOTH\n");return;
            }
            auto_last_metric_ms=millis()+(own_mac[5]%8)*85;
            auto_peer_enabled=auto_ctrl_target;
            og_auto_reset(&auto_policy,(uint32_t)millis());
            if(auto_enabled){
                auto_policy.changed_ms=(uint32_t)millis()-OG_AUTO_COOLDOWN_MS;
                auto_policy.started_ms=(uint32_t)millis()-OG_AUTO_PROMOTION_MS;
                auto_defer_until_ms=millis()+11000;
            }
            profile_report("ACTIVE");
            auto_report(auto_enabled?"ON":"OFF");
        }
        return;
    }
    if(f.type==OG_TYPE_AUTO_METRIC){
        if(f.data[5]!=profile_state.current ||
           (f.session==auto_last_metric_session && f.id<=auto_last_metric_id))return;
        auto_last_metric_session=f.session;auto_last_metric_id=f.id;
        auto_peer_enabled=f.data[6]!=0;
        auto_peer_recent_losses=f.data[7];
        if(auto_enabled&&auto_peer_enabled&&f.data[4]>=2){
            const int16_t rssi=(int16_t)((uint16_t)f.data[0]|(uint16_t)f.data[1]<<8);
            const int16_t snr=(int16_t)((uint16_t)f.data[2]|(uint16_t)f.data[3]<<8);
            og_auto_quality_note(&auto_policy.remote,rssi,snr,(uint32_t)millis());
        }
        return;
    }
    if(f.type==OG_TYPE_ARQ_CAPS){
        if(f.length==1 && (f.data[0]>=1 && f.data[0]<=3)){
            peer_arq_seen=true;peer_arq_at_ms=millis();
            peer_ultra_capable=f.data[0]>=3;
        }
        return;
    }
    if(f.type==OG_TYPE_PROFILE_OFFER){
        if(!profile_nonce_valid(&f))return;
        if(f.data[0]==OG_MODE_ULTRA && !peer_ultra_capable){
            og_frame_t reject=profile_frame(OG_TYPE_PROFILE_REJECT,f.data[0],f.session,f.id);
            (void)control_add(&reject,15);return;
        }
        /* Already authenticated by radio_hmac. The explicit operator consent
         * for BOTH radio boards is carried in the signed sixth byte. A peer
         * follows this manual request without needing a second USB command. */
        if(profile_state.phase==OG_PROFILE_IDLE && profile_has_pending()){
            if(og_profile_is_auto(&f) && auto_enabled &&
               f.data[0]>profile_state.current){
                /* Peer initiated an authenticated SAFETY downgrade, not a
                 * media boost. An old stalled transfer cannot indefinitely
                 * veto leaving a failing RF profile. The encrypted client
                 * outbox will requeue the canceled ciphertext message. */
                output("AUTO|PEER_PREEMPT|WEAK_LINK_ARQ|REQUEUE_IN_CLIENT\n");
                profile_reset_pending();
            }else{
                og_frame_t reject=profile_frame(OG_TYPE_PROFILE_REJECT,f.data[0],f.session,f.id);
                (void)control_add(&reject,30+(esp_random()%25));
                output("PROFILE|PEER_BUSY|%s\n",og_mode_name(f.data[0]));
                return;
            }
        }
        if(profile_state.phase==OG_PROFILE_IDLE){
            if(og_profile_is_auto(&f) && !auto_enabled){
                og_frame_t reject=profile_frame(OG_TYPE_PROFILE_REJECT,f.data[0],f.session,f.id);
                (void)control_add(&reject,10);return;
            }
            const bool media_offer=og_profile_is_media(&f);
            if(media_offer && f.data[0]!=OG_MODE_LOW &&
               (!media_boost_active || f.data[0]!=media_original_mode)){
                og_frame_t reject=profile_frame(OG_TYPE_PROFILE_REJECT,f.data[0],f.session,f.id);
                (void)control_add(&reject,10);return;
            }
            if(!og_profile_is_auto(&f) && !media_offer){
                auto_enabled=false;auto_peer_enabled=false;auto_ctrl_pending=false;
                media_boost_active=false;media_restore_requested=false;
                output("AUTO|OFF|MANUAL_PEER\n");
            }
            if(media_offer && f.data[0]==OG_MODE_LOW && !media_boost_active){
                media_boost_active=true;media_original_mode=profile_state.current;
                media_active_until_ms=millis()+10*60*1000;
            }
            auto_requested_profile=og_profile_is_auto(&f);
            media_requested_profile=media_offer;
            if(last_profile_offer_valid && f.id==last_profile_offer_nonce &&
               f.session==last_profile_offer_session &&
               !memcmp(f.sender,last_profile_offer_mac,6))return;
            last_profile_offer_valid=true;
            last_profile_offer_nonce=f.id;
            last_profile_offer_session=f.session;
            memcpy(last_profile_offer_mac,f.sender,6);
            profile_verify_initiator=false;
            profile_state.previous=profile_state.current;
            profile_state.target=f.data[0];
            profile_state.nonce=f.id;profile_state.offer_session=f.session;
            profile_state.phase=OG_PROFILE_RESPONDING;
            profile_state.retry_at_ms=millis()+
                (profile_state.current==OG_MODE_ULTRA?12000:3500);
            output("PROFILE|PEER_REQUEST|%s\n",og_mode_name(profile_state.target));
        }
        if(profile_state.phase==OG_PROFILE_RESPONDING &&
           profile_state.nonce==f.id && profile_state.offer_session==f.session &&
           profile_state.target==f.data[0]){
            og_frame_t ack=profile_frame(OG_TYPE_PROFILE_AGREE,profile_state.target,f.session,f.id);
            (void)control_add(&ack,30+(esp_random()%25));
        }
        return;
    }
    if(f.type==OG_TYPE_PROFILE_REJECT){
        if(profile_state.phase==OG_PROFILE_OFFERING && profile_nonce_valid(&f) &&
           profile_state.nonce==f.id && profile_state.offer_session==f.session &&
           profile_state.target==f.data[0]){
            profile_state.phase=OG_PROFILE_IDLE;
            output("PROFILE|ERROR|PEER_BUSY_FINISH_TRANSFERS_AND_RETRY\n");
            if(media_requested_profile && profile_state.target==OG_MODE_LOW){
                media_boost_active=false;
                output("MEDIA|BYPASS|PEER_BUSY\n");
            }
        }
        return;
    }
    if(f.type==OG_TYPE_PROFILE_AGREE){
        if(profile_state.phase==OG_PROFILE_OFFERING && profile_nonce_valid(&f) &&
           profile_state.nonce==f.id && profile_state.offer_session==f.session &&
           profile_state.target==f.data[0] &&
           og_profile_is_auto(&f)==auto_requested_profile &&
           og_profile_is_media(&f)==media_requested_profile){
            profile_state.phase=OG_PROFILE_WAIT_SWITCH;
            profile_state.switch_at_ms=millis()+OG_PROFILE_SWITCH_GRACE_MS;
            output("PROFILE|AGREED|%s\n",og_mode_name(profile_state.target));
        }
        return;
    }
    if(f.type==OG_TYPE_RADIO_ACK) {
        for(size_t i=0;i<OG_PENDING;i++)if(pending[i].used &&
            pending[i].frame.session==f.session && pending[i].frame.id==f.id){
            if(!pending[i].radio_acked){
                pending[i].radio_acked=true;stat_radio_ack++;
                output("RADIOACK|%" PRIu32 "|%" PRIu32 "\n",f.session,f.id);
                output("TRACE|RADIO_ACK|%" PRIu32 "|%" PRIu32 "|RTT_MS=%" PRId64 "\n",
                       f.session,f.id,millis()-pending[i].tx_at_ms);
                /* Radio reliability is complete. Application reliability is
                 * provided separately by encrypted OGM4 delivered/read
                 * receipts. Free the slot for long multi-fragment text. */
                pending[i].used=false;
            }
            break;
        }
        return;
    }
    /* Give all DATA and application-control frames a MAC-layer radio ACK.
     * Radio ACK is not an authenticated application delivery receipt. */
    og_frame_t ack=new_frame(OG_TYPE_RADIO_ACK,f.session,f.id);
    (void)control_add(&ack,35+(esp_random()%30));
    /* OG_TYPE_DELIVERED/READ are legacy radio events, never interpreted by
     * the v1.1 web app as an encrypted double-gray/blue receipt. */
    if(f.type==OG_TYPE_DELIVERED || f.type==OG_TYPE_READ)return;
    og_received_t *r=history_find(f.sender,f.session,f.id);
    if(r){
        if(!r->app_acked && millis()-r->forwarded_ms>800){
            incoming_browser(&f);r->forwarded_ms=millis();
        }
        return;
    }
    r=&received[received_next++%OG_RX_HISTORY];memset(r,0,sizeof(*r));
    r->used=true;memcpy(r->sender,f.sender,6);r->session=f.session;
    r->id=f.id;r->forwarded_ms=millis();
    incoming_browser(&f);
}
static void queue_data(const og_command_t *c) {
    if(profile_state.phase!=OG_PROFILE_IDLE || og_arq_bridge_busy(&arq)){
        output("ERROR|PROFILE_SWITCH_OR_ARQ_BUSY\n");return;
    }
    if(!radio_key_ready){output("ERROR|KEY_REQUIRED\n");return;}
    og_pending_t *p=NULL;
    /* Keep two slots available for encrypted delivered/read receipts even
     * when multiple regular text fragments have been queued. */
    if(c->type==CMD_DATA){
        unsigned ordinary=0;
        for(size_t i=0;i<OG_PENDING;i++)if(pending[i].used && !pending[i].urgent)ordinary++;
        if(ordinary>=OG_PENDING-2){output("ERROR|QUEUE_FULL\n");return;}
    }
    for(size_t i=0;i<OG_PENDING;i++)if(!pending[i].used){p=&pending[i];break;}
    if(!p){output("ERROR|QUEUE_FULL\n");return;}
    memset(p,0,sizeof(*p));p->used=true;p->urgent=c->type==CMD_URGENT;
    p->frame=new_frame(OG_TYPE_DATA,boot_session,next_id++);
    p->frame.length=c->length;memcpy(p->frame.data,c->data,c->length);
    p->due_ms=millis()+25+(esp_random()%75);
    if(c->ble_binary){p->used=false;offgrid_ble_publish("ERROR|BINARY_NOT_YET_SUPPORTED_IN_LORA_V1");}
    else {
        char prefix[100];snprintf(prefix,sizeof(prefix),"QUEUED|%" PRIu32 "|%" PRIu32 "|",p->frame.session,p->frame.id);
        hex_record(prefix,c->data,c->length);
    }
}
static void process_command(const og_command_t *c) {
    if(c->type==CMD_STATUS){show_ready("STATUS");show_link();profile_report(profile_state.phase==OG_PROFILE_IDLE?"ACTIVE":"TRANSITION");auto_report(auto_enabled?"ON":"OFF");output("MEDIA|STATUS|ACTIVE=%u|BOOST=%u|RETURN=%s\n",(unsigned)(media_active_until_ms>millis()),(unsigned)media_boost_active,og_mode_name(media_boost_active?media_original_mode:profile_state.current));return;}
    if(c->type==CMD_PROFILE){profile_start_offer(c->profile_target,c->force_profile,false,false);return;}
    if(c->type==CMD_AUTO_ON || c->type==CMD_AUTO_OFF){auto_command(c->type==CMD_AUTO_ON);return;}
    if(c->type==CMD_MEDIA_OFF){
        media_active_until_ms=0;
        if(media_boost_active && profile_state.current!=media_original_mode){
            /* Do not abort the final radio bitmap ACK or encrypted receipt.
             * media_service() will negotiate the original profile once idle. */
            media_restore_requested=true;
            media_restore_retry_ms=0;
            output("MEDIA|RESTORE_PENDING|%s\n",og_mode_name(media_original_mode));
        }else{
            if(profile_state.phase==OG_PROFILE_IDLE)media_boost_active=false;
            output("MEDIA|OFF|PROFILE=%s\n",og_mode_name(profile_state.current));
        }
        return;
    }
    if(c->type==CMD_MEDIA_ON){
        const int64_t now=millis();
        media_active_until_ms=now+10*60*1000;
        if(profile_state.phase!=OG_PROFILE_IDLE){
            output("MEDIA|ERROR|PROFILE_TRANSITION_IN_PROGRESS\n");return;
        }
        if(profile_state.current==OG_MODE_ULTRA){
            output("MEDIA|ERROR|ULTRA_TEXT_ONLY\n");return;
        }
        if(profile_state.current==OG_MODE_LOW){
            output("MEDIA|READY|MODE=LOW\n");return;
        }
        if(media_boost_active){
            output("MEDIA|BYPASS|PREVIOUS_MEDIA_MODE_UNSETTLED\n");return;
        }
        /* In Auto, avoid forcing a weak two-way link into Fast. In manual
         * mode, the authenticated profile handshake/rollback is the safety
         * gate, with the operator's explicit both-board PA authorization. */
        if(!high_pa_verified){output("MEDIA|BYPASS|POWER18_NOT_VERIFIED\n");return;}
        if(profile_state.phase!=OG_PROFILE_IDLE || profile_has_pending() ||
           !peer_seen || now-peer_at_ms>OG_PEER_STALE_MS){
            output("MEDIA|BYPASS|PEER_OR_RADIO_BUSY\n");return;
        }
        const bool both_fresh=og_auto_fresh(&auto_policy.local,(uint32_t)now) &&
                              og_auto_fresh(&auto_policy.remote,(uint32_t)now);
        /* Do not override the EARLY weak-link adaptation by forcing media
         * into Fast. Auto requires valid reports from BOTH directions;
         * a poor local report also blocks the boost in manual mode. */
        const bool local_weak=og_auto_fresh(&auto_policy.local,(uint32_t)now) &&
            (auto_policy.local.rssi_dbm<=OG_AUTO_FAST_DOWN_RSSI ||
             auto_policy.local.snr_q4<=OG_AUTO_FAST_DOWN_SNR_Q4);
        const bool remote_weak=og_auto_fresh(&auto_policy.remote,(uint32_t)now) &&
            (auto_policy.remote.rssi_dbm<=OG_AUTO_FAST_DOWN_RSSI ||
             auto_policy.remote.snr_q4<=OG_AUTO_FAST_DOWN_SNR_Q4);
        if(local_weak || (auto_enabled &&
           (!both_fresh || remote_weak || auto_peer_recent_losses>=2))){
            output("MEDIA|BYPASS|FAST_LINK_MARGIN_INSUFFICIENT\n");return;
        }
        media_original_mode=profile_state.current;
        media_boost_active=true;
        media_restore_requested=false;
        media_restore_attempts=0;
        output("MEDIA|BOOST_REQUEST|LOW|RETURN=%s\n",og_mode_name(media_original_mode));
        profile_start_offer(OG_MODE_LOW,false,false,true);
        return;
    }
    if(c->type==CMD_HIGH_PA_ON){
        high_pa_verified=true;
        output("POWER18|ENABLED|REQUIRES_STABLE_3V3\n");return;
    }
    if(c->type==CMD_HIGH_PA_OFF){
        if(auto_enabled || profile_state.current==OG_MODE_LOW ||
           profile_state.current==OG_MODE_MAX || profile_state.current==OG_MODE_ULTRA){
            output("POWER18|ERROR|DISABLE_AUTO_AND_LEAVE_HIGH_POWER_FIRST\n");return;
        }
        high_pa_verified=false;output("POWER18|DISABLED\n");return;
    }
    if(c->type==CMD_BULK_BEGIN){
        if(!radio_key_ready||profile_state.phase!=OG_PROFILE_IDLE||recovery.probing){
            output("BULKFAILED|SESSION|NOT_READY_OR_PROFILE_SWITCH_OR_RECOVERY\n");return;
        }
        /* Stage ciphertext even before initial peer discovery; the radio
         * worker MUST NOT send ARQ DATA until authenticated v1.6 CAPS.
         * This avoids spuriously failing a message on immediate connect. */
        if(profile_state.current==OG_MODE_ULTRA){
            if(c->bulk_media){output("BULKFAILED|SESSION|ULTRA_TEXT_ONLY\n");return;}
            /* A full 500 Unicode-character text can exceed 1 KB ciphertext.
             * Never truncate: choose the smallest supported RF payload that
             * holds ALL fragments within the authenticated 32-bit ACK bitmap. */
            const bool weak=auto_policy.local.rssi_dbm<=-127 ||
                 auto_policy.local.snr_q4<=-48;
            const uint8_t preferred=weak?48:64;
            uint8_t payload=preferred;
            while(payload<96 && (size_t)c->length>32u*payload)payload+=16;
            if((size_t)c->length>32u*payload){
                output("BULKFAILED|SESSION|ULTRA_TEXT_TOO_LONG\n");return;
            }
            const uint8_t window=weak?1:2;
            og_arq_bridge_set_ultra(&arq,true,payload,window);
        }else og_arq_bridge_set_ultra(&arq,false,OG_SR_PAYLOAD,OG_SR_MAX_WINDOW);
        (void)og_arq_bridge_begin(&arq,c->bulk_session,c->bulk_transfer,
                                  c->length,c->bulk_urgent,&arq_io);return;
    }
    if(c->type==CMD_BULK_CHUNK){
        (void)og_arq_bridge_chunk(&arq,c->bulk_session,c->bulk_transfer,
                                  c->ble_token,c->data,c->length,&arq_io);return;
    }
    if(c->type==CMD_BULK_END){
        if(og_arq_bridge_end(&arq,c->bulk_session,c->bulk_transfer,
                             c->bulk_crc,&arq_io)){
            /* A 1-KB ciphertext now fits one 6-frame SF7 bitmap window,
             * replacing the needless 4+1 split/extra ACK in rc.1.
             * Range mode uses a smaller window for long-airtime packets. */
            if(profile_state.current!=OG_MODE_ULTRA)
                arq.tx.window=profile_state.current==OG_MODE_MAX?3u:6u;
            arq_last_tx_ms=millis();
        }
        return;
    }
    if(c->type==CMD_KEY){derive_radio_key(c->key);output("KEYRADIO|%s\n",radio_key_ready?"READY":"ERROR");return;}
    if(c->type==CMD_DATA||c->type==CMD_URGENT){queue_data(c);return;}
    if(!radio_key_ready){output("ERROR|KEY_REQUIRED\n");return;}
    if(c->type==CMD_APPACK){
        og_received_t *r=history_find(c->mac,c->session,c->id);
        if(!r){output("ERROR|UNKNOWN_APPACK\n");return;}
        if(!r->app_acked){
            r->app_acked=true;stat_appack++;stat_delivered++;
            output("TRACE|APP_VALIDATED|%" PRIu32 "|%" PRIu32 "|WAIT_MS=%" PRId64 "\n",
                   c->session,c->id,millis()-r->forwarded_ms);
        }
        /* No second over-air ACK: one radio ACK per fragment, one encrypted
         * application receipt per fully decrypted message in the browser. */
    } else if(c->type==CMD_READ){
        /* Legacy command: never forge a blue tick. The v1.1 web app sends
         * encrypted read receipts as priority CTRLHEX data instead. */
        output("ERROR|USE_ENCRYPTED_READ_RECEIPT\n");
    }
}
/* One outstanding unacknowledged RF DATA frame at a time. The browser may
 * pipeline up to three fragments into the firmware queue, but the transmitter
 * ALWAYS opens a receive window for a packet's radio ACK before another DATA
 * transmit. This avoids half-duplex self-collisions as window size increases. */
static og_pending_t *choose_pending(int64_t now) {
    for(size_t i=0;i<OG_PENDING;i++){
        og_pending_t *p=&pending[i];
        if(p->used && p->tries && !p->radio_acked)
            return (now>=p->due_ms)?p:NULL;
    }
    for(int urgent=1;urgent>=0;urgent--)
        for(size_t i=0;i<OG_PENDING;i++){
            og_pending_t *p=&pending[i];
            if(p->used && p->urgent==(urgent!=0) && p->due_ms<=now &&
               p->tries==0)return p;
        }
    return NULL;
}
static void service_queues(void) {
    const int64_t now=millis();
    /* ACKs preempt user payloads. HELLO only when not waiting for an ACK. */
    bool waiting_for_ack=false;
    for(size_t i=0;i<OG_PENDING;i++)
        if(pending[i].used && pending[i].tries>0 && !pending[i].radio_acked)
            waiting_for_ack=true;
    for(int priority=0;priority<3;priority++){
        for(size_t i=0;i<OG_CTRL_QUEUE;i++){
            const og_frame_t *f=&controls[i].frame;
            if(!f->magic || now<controls[i].due_ms)continue;
            if(profile_state.phase==OG_PROFILE_VERIFYING ||
               (profile_state.phase!=OG_PROFILE_IDLE &&
                f->type!=OG_TYPE_RADIO_ACK && f->type!=OG_TYPE_PROFILE_AGREE &&
                f->type!=OG_TYPE_PROFILE_REJECT))continue;
            const int rank=(f->type==OG_TYPE_RADIO_ACK)?0:
                           (f->type==OG_TYPE_PROFILE_AGREE || f->type==OG_TYPE_PROFILE_REJECT)?1:
                           (f->type==OG_TYPE_HELLO?2:1);
            if(rank!=priority || (waiting_for_ack && rank!=0))continue;
            if(arq.tx_used && peer_arq_seen && f->type==OG_TYPE_ARQ_CAPS)continue;
            /* Discovery cannot consume airtime while real peer traffic is
             * flowing. It is disposable, not a delivery guarantee. */
            if(rank==2 && (arq.tx_used || arq.usb.active || (peer_seen && now-peer_at_ms<2000))){
                controls[i].frame.magic=0;continue;
            }
            const og_frame_t frame=*f;controls[i].frame.magic=0;
            const esp_err_t result=tx(&frame);
            if(result==ESP_OK && frame.type==OG_TYPE_PROFILE_AGREE &&
               profile_state.phase==OG_PROFILE_RESPONDING &&
               frame.id==profile_state.nonce){
                profile_state.phase=OG_PROFILE_WAIT_SWITCH;
                profile_state.switch_at_ms=millis()+OG_PROFILE_SWITCH_GRACE_MS;
                output("PROFILE|AGREED|%s\n",og_mode_name(profile_state.target));
            }
            return;
        }
    }
    /* Never start a long data transmission with an imminent ACK waiting. */
    for(size_t i=0;i<OG_CTRL_QUEUE;i++){
        if(controls[i].frame.magic && controls[i].frame.type==OG_TYPE_RADIO_ACK)
            return;
    }
    if(profile_state.phase!=OG_PROFILE_IDLE || arq.tx_used || arq.usb.active)return;
    og_pending_t *p=choose_pending(now);
    if(!p)return;
    if(p->tries>=OG_TX_ATTEMPTS){
        output("FAILED|%" PRIu32 "|%" PRIu32 "|NO_RADIO_ACK\n",
               p->frame.session,p->frame.id);
        stat_fail++;p->used=false;return;
    }
    if(p->tries>0){
        stat_retry++;
        output("RETRY|%" PRIu32 "|%" PRIu32 "|%u\n",
               p->frame.session,p->frame.id,(unsigned)(p->tries+1));
    }
    /* v1.2.0-rc.2: do NOT call CAD on the active continuous receiver.
     * The v1.2 rc.1 pre-TX modem-status/CAD gate could defer a pending
     * packet indefinitely, even when the underlying v1.1 TX path worked.
     * Revert to the known-working v1.1 send path while keeping the v1.2
     * browser serialization, ACK priority and encrypted receipt batching.
     * The SX1278 owner task still transmits at most one unacknowledged
     * DATA frame at a time; delivery is not asserted without a receipt.
     */
    esp_err_t status=tx(&p->frame);
    p->tries++;
    if(status==ESP_OK){
        stat_tx_done++;
        /* Single tick: SX1278 TxDone, not radio ACK or receipt. */
        p->tx_at_ms=millis();
        if(!p->tx_notified){p->tx_notified=true;
            output("SENT|%" PRIu32 "|%" PRIu32 "\n",p->frame.session,p->frame.id);
            output("TRACE|TX_DONE|%" PRIu32 "|%" PRIu32 "|AT_MS=%" PRId64 "\n",
                   p->frame.session,p->frame.id,p->tx_at_ms);
        }
    }
    /* Restore the tested v1.1 retransmission policy, without the new CAD
     * state changes. Packet retries are still bounded and jittered. */
    const uint32_t contention=p->tries<=1?120u:180u*(uint32_t)p->tries;
    p->due_ms=millis()+og_mode_ack_wait_ms(profile_state.current,p->tries)+(esp_random()%contention);
    if(status!=ESP_OK && p->tries>=OG_TX_ATTEMPTS){
        output("FAILED|%" PRIu32 "|%" PRIu32 "|RADIO_TX_ERROR\n",
               p->frame.session,p->frame.id);
        stat_fail++;p->used=false;
    }
}
static void radio_loop(void *arg) {
    (void)arg;
    if(psa_crypto_init()!=PSA_SUCCESS){output("FATAL|PSA_INIT\n");vTaskDelete(NULL);return;}
    og_arq_bridge_init(&arq);
    last_hello_ms=-OG_DISCOVERY_HELLO_MS; /* send authenticated HELLO soon after boot */
    offgrid_sx1278_profile_t profile=og_mode_profile(OG_MODE_BALANCED);
    offgrid_sx1278_pins_t pins=offgrid_sx1278_default_pins();
    esp_err_t e=offgrid_sx1278_open(&pins,&profile,&sx);
    if(e!=ESP_OK){output("FATAL|SX1278_INIT_FAILED|%s\n",esp_err_to_name(e));vTaskDelete(NULL);return;}
    output("RADIO|SX1278_READY|433000000|SF7|BW250000|CR4/5|TX_DBM=10\n");
    profile_report("ACTIVE");
    show_ready("RADIO_READY");
    /* Start hearing immediately, never stop RX at every short polling timeout. */
    for(;;){
        og_command_t cmd;
        for(int count=0;count<6 && xQueueReceive(commands,&cmd,0)==pdTRUE;count++)
            process_command(&cmd);
        uint8_t bytes[OFFGRID_SX1278_MAX_PAYLOAD];size_t n=0;
        offgrid_sx1278_packet_info_t info={0};
        const uint32_t poll_ms=(arq.tx_used && arq.tx.phase==OG_SR_MORE)?2:OG_RX_POLL_MS;
        e=offgrid_sx1278_receive(sx,bytes,sizeof(bytes),&n,&info,poll_ms);
        if(e==ESP_OK){
            if(n && bytes[0]==OG_SR_VERSION){
                const uint32_t was=arq.reject_auth;
                uint32_t silence=profile_state.current==OG_MODE_ULTRA?350u:
                                 profile_state.current==OG_MODE_MAX?1950u:
                                 profile_state.current==OG_MODE_BALANCED?480u:200u;
                (void)og_arq_bridge_receive(&arq,bytes,n,(uint32_t)millis(),silence,&arq_io);
                if(arq.reject_auth==was){
                    if(recovery.probing){
                        (void)og_recovery_step(&recovery,(uint32_t)millis(),0,
                              profile_state.current,false,true);
                        recovery_locks++;
                        peer_arq_seen=false;peer_ultra_capable=false;last_arq_caps_ms=0;
                        auto_peer_enabled=false;auto_last_metric_session=0;auto_last_metric_id=0;
                        auto_defer_until_ms=millis()+OG_RECOVERY_AUTO_DEFER_MS;
                        output("RECOVERY|AUTHENTICATED|BALANCED|ARQ_PEER_RESTORED\n");
                        profile_report("ACTIVE");
                    }
                    peer_at_ms=millis();peer_arq_at_ms=millis();
                    stat_rx++;peer_rssi=info.rssi_dbm;arq_auth_rx++;
                    og_auto_quality_note(&auto_policy.local,info.rssi_dbm,
                        (int16_t)(info.snr_db*4.0f),(uint32_t)millis());
                }
                else stat_badtag++;
            }else received_frame(bytes,n,info.rssi_dbm,(int16_t)(info.snr_db*4.0f));
        }
        else if(e==ESP_ERR_INVALID_RESPONSE)stat_crc++;
        int64_t now=millis();
        profile_service();
        media_service();
        auto_service();
        link_recovery_service();
        const int64_t hello_interval=(recovery.probing ||
             (peer_seen && now-peer_at_ms>OG_PEER_STALE_MS &&
              profile_state.current==OG_MODE_BALANCED))?
            OG_RECOVERY_BEACON_MS:profile_state.current==OG_MODE_ULTRA?
            24000:(peer_seen?OG_HELLO_MS:OG_DISCOVERY_HELLO_MS);
        if(profile_state.phase==OG_PROFILE_IDLE && now>=profile_assist_until_ms && radio_key_ready && now-last_hello_ms>=hello_interval){
            last_hello_ms=now+(esp_random()%450);
            bool busy=false;
            for(size_t i=0;i<OG_PENDING;i++)if(pending[i].used){busy=true;break;}
            if(peer_seen && now-peer_at_ms<2000)busy=true;
            if(peer_arq_seen && og_arq_bridge_busy(&arq))busy=true;
            if(!busy){
                og_frame_t hello=new_frame(OG_TYPE_HELLO,boot_session,0);
                (void)control_add(&hello,110+(esp_random()%300));
            }
        }
        /* Version capability is authenticated over the working OGM2 control
         * profile BEFORE any v1.6 SR transfer is accepted. Old v1.5 peers
         * cannot fake this and will never silently consume new wire data. */
        if(profile_state.phase==OG_PROFILE_IDLE && radio_key_ready && peer_seen &&
           now-last_arq_caps_ms >= (profile_state.current==OG_MODE_ULTRA?45000:
                                     peer_arq_seen?15000:1100)){
            last_arq_caps_ms=now;
            og_frame_t cap=new_frame(OG_TYPE_ARQ_CAPS,boot_session,0);
            cap.length=1;cap.data[0]=3; /* v3.0.5 Ultra CR4/8 peer */
            (void)control_add(&cap,25+(esp_random()%30));
        }
        if(now-last_link_ms>=OG_STATUS_MS){last_link_ms=now;show_link();}
        /* The USB READY beacon is for reconnect, not a source of continuous
         * serial pressure during sustained high-volume transfers. */
        if(now-last_ready_ms>=8000){last_ready_ms=now;show_ready("BEACON");}
        /* SR bitmap ACKs MUST preempt old HELLO/CAPS and any long outgoing
         * transfer; all RF work remains serialized in this owner task. */
        char peer[18];mac_text(peer_mac,peer);
        const uint32_t silence=profile_state.current==OG_MODE_ULTRA?350u:
                               profile_state.current==OG_MODE_MAX?1950u:
                               profile_state.current==OG_MODE_BALANCED?480u:200u;
        const uint32_t ack_timeout=profile_state.current==OG_MODE_ULTRA?
            og_ultra_ack_wait_ms(OG_SR_HEADER+OG_SR_ACK_BYTES+OG_SR_TAG,
                 og_sr_tx_largest_attempt(&arq.tx)) :
                                   profile_state.current==OG_MODE_MAX?2250u:
                                   profile_state.current==OG_MODE_BALANCED?750u:420u;
        bool legacy_busy=false;
        for(size_t i=0;i<OG_PENDING;i++)if(pending[i].used)legacy_busy=true;
        /* If this is an older peer, avoid transmitting incompatible ARQ
         * frames. Explicitly fail after a bounded discovery period. */
        if(arq.tx_used && !peer_arq_seen &&
           millis()-arq_last_tx_ms>12000 && arq_last_tx_ms){
            og_arq_bridge_cancel(&arq,&arq_io,"PEER_REQUIRES_V16_ARQ");
        }
        const bool peer_ready=peer_arq_seen &&
            (millis()-peer_arq_at_ms<OG_PEER_STALE_MS ||
             (arq.tx_used && arq.tx.transmitted!=0));
        const bool urgent=og_arq_bridge_service(&arq,(uint32_t)millis(),esp_random(),
                               silence,ack_timeout,false,peer,&arq_io);
        if(!urgent && !recovery.probing){
            const uint32_t earlier=stat_tx+stat_tx_errors;
            service_queues();
            if(earlier==stat_tx+stat_tx_errors){
                (void)og_arq_bridge_service(&arq,(uint32_t)millis(),esp_random(),
                     silence,ack_timeout,profile_state.phase==OG_PROFILE_IDLE&&!legacy_busy&&peer_ready,
                     peer,&arq_io);
            }
        }
        /* Never spin continuously even when both USB and LoRa are active. */
        vTaskDelay(1);
    }
}

#if CONFIG_OFFGRID_USB_IRQ_RX
static void setup_usb(void) {
    usb_serial_jtag_driver_config_t cfg={.rx_buffer_size=4096,.tx_buffer_size=4096};
    const esp_err_t e=usb_serial_jtag_driver_install(&cfg);
    if(e==ESP_OK || e==ESP_ERR_INVALID_STATE){OG_USB_VFS_USE_DRIVER();usb_irq_rx=true;puts("USBMODE|IRQ");}
    else output("USBMODE|LEGACY|%s\n",esp_err_to_name(e));
}
#endif
static int usb_read(uint8_t *b,size_t cap) {
#if CONFIG_OFFGRID_USB_IRQ_RX
    if(usb_irq_rx)return usb_serial_jtag_read_bytes(b,(uint32_t)cap,pdMS_TO_TICKS(80)+1);
#endif
    int ch=getchar();if(ch==EOF){clearerr(stdin);vTaskDelay(1);return 0;}
    b[0]=(uint8_t)ch;return 1;
}
static bool queue_command(const og_command_t *c) {
    if(xQueueSend(commands,c,pdMS_TO_TICKS(100)+1)!=pdTRUE){output("ERROR|EVENT_QUEUE_FULL\n");return false;}
    return true;
}
static void parse_line(const char *line,bool ble) {
    og_command_t c={0};
    if(!strcmp(line,"STATUS")){c.type=CMD_STATUS;queue_command(&c);return;}
    if(!strcmp(line,"HELP")){output("HELP|BULKBEGIN,BULKCHUNK,BULKEND,STATUS,PROFILE LOW/BALANCED/MAX/ULTRA,AUTO ON/OFF,MEDIA ON/OFF,POWER18 VERIFIED,KEYGET,KEYSET,KEYSTATUS,CAPS\n");return;}
    if(!strcmp(line,"CAPS") || !strcmp(line,"CAPS?")){
        output("CAPS|FAMILY=LORA|PROTO=2|SX1278=1|PROFILE=AUTO4_ONE_CLICK|ARQ=1|BULK=1|TEXT=1|PHOTO=1|VOICE=1|ULTRA=SF12_TEXT_ONLY|WIFI=0|BLE=1\n");return;
    }
    if(!strcmp(line,"POWER18 VERIFIED")){
        /* BLE calls reach this handler only after challenge/response HMAC auth.
         * The browser UI requires an explicit both-board PA18 confirmation. */
        /* Queue to radio worker; cross-task state is never mutated in BLE/USB. */
        c.type=CMD_HIGH_PA_ON;queue_command(&c);return;
    }
    if(!strcmp(line,"POWER18 OFF")){
        c.type=CMD_HIGH_PA_OFF;queue_command(&c);return;
    }
    if(!strcmp(line,"MEDIA ON")||!strcmp(line,"MEDIA OFF")){
        c.type=!strcmp(line,"MEDIA ON")?CMD_MEDIA_ON:CMD_MEDIA_OFF;
        queue_command(&c);return;
    }
    if(!strcmp(line,"AUTO ON")||!strcmp(line,"AUTO OFF")){
        c.type=!strcmp(line,"AUTO ON")?CMD_AUTO_ON:CMD_AUTO_OFF;
        queue_command(&c);return;
    }
    if(!strncmp(line,"PROFILE ",8)){
        char name[20]={0},extra[16]={0};
        const int fields=sscanf(line+8,"%19s %15s",name,extra);
        const uint8_t mode=og_mode_parse(name);
        if(!mode||fields<1||(fields==2&&strcmp(extra,"FORCE"))){
            output("PROFILE|ERROR|INVALID_MODE\n");return;
        }
        c.type=CMD_PROFILE;c.profile_target=mode;
        c.force_profile=(fields==2);
        queue_command(&c);return;
    }
    if(!strcmp(line,"KEYSTATUS")){output("KEYSTATUS|%s\n",key_present?"PRESENT":"EMPTY");return;}
    if(!strcmp(line,"KEYGET")){
        if(ble){output("ERROR|BLE_KEY_EXPORT_DENIED\n");return;}
        print_key();return;
    }
    if(!strncmp(line,"KEYSET ",7)){
        if(ble){output("ERROR|BLE_KEYSET_DENIED\n");return;}
        uint8_t raw[OG_MAX_DATA];uint16_t n=0;
        if(strlen(line+7)!=64 || !parse_hex(line+7,raw,&n)||n!=32){puts("KEYERROR|INVALID_HEX");return;}
        if(!nvs_key_save(raw)){puts("KEYERROR|NVS_WRITE_FAILED");return;}
        memcpy(c.key,raw,32);c.type=CMD_KEY;
        if(queue_command(&c))puts("KEYSTORED");
        memset(raw,0,sizeof(raw));return;
    }
    /* v1.6 USB bulk: one AES-GCM ciphertext, not dozens of browser-side
     * 160-byte radio frames. Each staged USB fragment has explicit seq/CRC.
     * The radio task provides positive per-stage backpressure. */
    if(!strncmp(line,"BULKBEGIN ",10)){
        char id[20]={0},kind[12]={0},junk=0;unsigned len=0;
        if(sscanf(line+10,"%19s %u %11s %c",id,&len,kind,&junk)!=3 ||
           len==0 || len>OG_SR_MAX_TRANSFER ||
           (strcmp(kind,"DATA")&&strcmp(kind,"CTRL")&&strcmp(kind,"MEDIA")) ||
           !bulk_hex_id(id,&c.bulk_session,&c.bulk_transfer)){
            output("ERROR|BAD_BULK_BEGIN\n");return;
        }
        c.type=CMD_BULK_BEGIN;c.length=(uint16_t)len;
        c.bulk_urgent=!strcmp(kind,"CTRL");
        c.bulk_media=!strcmp(kind,"MEDIA");queue_command(&c);return;
    }
    if(!strncmp(line,"BULKCHUNK ",10)){
        char id[20]={0};unsigned seq=0;int pos=0;char junk=0;
        if(sscanf(line+10,"%19s %u %n",id,&seq,&pos)!=2 ||
           !pos || seq>UINT16_MAX ||
           !bulk_hex_id(id,&c.bulk_session,&c.bulk_transfer) ||
           !parse_hex(line+10+pos,c.data,&c.length) ||
           c.length>OG_BRIDGE_STAGE_CHUNK ||
           (sscanf(line+10+pos,"%*s %c",&junk)==1)){
            output("ERROR|BAD_BULK_CHUNK\n");return;
        }
        c.type=CMD_BULK_CHUNK;c.ble_token=(uint16_t)seq;
        queue_command(&c);return;
    }
    if(!strncmp(line,"BULKEND ",8)){
        char id[20]={0},crc[12]={0},junk=0;unsigned value=0;
        if(sscanf(line+8,"%19s %11s %c",id,crc,&junk)!=2 ||
           strlen(crc)!=8 || sscanf(crc,"%8x%c",&value,&junk)!=1 ||
           !bulk_hex_id(id,&c.bulk_session,&c.bulk_transfer)){
            output("ERROR|BAD_BULK_END\n");return;
        }
        c.type=CMD_BULK_END;c.bulk_crc=(uint32_t)value;queue_command(&c);return;
    }
    if(!strncmp(line,"MSGHEX ",7)||!strncmp(line,"CTRLHEX ",8)){
        const bool urgent=line[0]=='C';
        c.type=urgent?CMD_URGENT:CMD_DATA;
        if(!parse_hex(line+(urgent?8:7),c.data,&c.length)){
            output("ERROR|INVALID_HEX_OR_LENGTH\n");return;
        }
        queue_command(&c);return;
    }
    if(!strncmp(line,"APPACK ",7)||!strncmp(line,"READ ",5)){
        const bool read=line[0]=='R';char mac[32],tail;
        unsigned long session=0,id=0;
        if(sscanf(line+(read?5:7),"%31s %lu %lu %c",mac,&session,&id,&tail)!=3 ||
           session>UINT32_MAX || id>UINT32_MAX || !mac_parse(mac,c.mac)){
            output("ERROR|BAD_ACK\n");return;
        }
        c.type=read?CMD_READ:CMD_APPACK;c.session=(uint32_t)session;c.id=(uint32_t)id;
        queue_command(&c);return;
    }
    if(*line)output("ERROR|UNKNOWN_COMMAND\n");
}
static void serial_loop(void *arg) {
    (void)arg;char line[OG_SERIAL_BUF];size_t used=0;uint8_t buffer[256];
    for(;;){
        const int count=usb_read(buffer,sizeof(buffer));
        for(int i=0;i<count;i++){
            const int b=buffer[i];if(b=='\r')continue;
            if(b=='\n') {line[used]=0;parse_line(line,false);used=0;}
            else if(used+1<sizeof(line))line[used++]=(char)b;
            else {used=0;output("ERROR|LINE_TOO_LONG\n");}
        }
    }
}
static void ble_commands(void *arg) {
    (void)arg;offgrid_ble_command_t cmd;
    for(;;){
        if(!offgrid_ble_receive_command(&cmd)){vTaskDelay(pdMS_TO_TICKS(15)+1);continue;}
        if(cmd.kind==OFFGRID_BLE_COMMAND_TEXT)parse_line(cmd.data.text,true);
        else if(cmd.kind==OFFGRID_BLE_COMMAND_BINARY){
            /* Future Android universal support can use binary GATT packets.
             * v1.0 intentionally does not advertise binary mode support. */
            offgrid_ble_publish("ERROR|LORA_BINARY_NOT_AVAILABLE_V1");
        }
    }
}
void app_main(void) {
    setvbuf(stdout,NULL,_IOLBF,0);
#if CONFIG_OFFGRID_USB_IRQ_RX
    setup_usb();
#else
    puts("USBMODE|LEGACY");
#endif
    puts("BOOT|OffGrid Messenger LoRa v1.6.1-rc.2 (Wi-Fi and ESP-NOW excluded)");
    esp_err_t e=nvs_flash_init();
    if(e==ESP_ERR_NVS_NO_FREE_PAGES || e==ESP_ERR_NVS_NEW_VERSION_FOUND){
        /* This is a standard ESP-IDF NVS recovery path. It erases NVS only
         * when the partition itself cannot be opened, never on normal boot. */
        ESP_ERROR_CHECK(nvs_flash_erase());e=nvs_flash_init();
    }
    ESP_ERROR_CHECK(e);
    ESP_ERROR_CHECK(esp_read_mac(own_mac,ESP_MAC_WIFI_STA)); /* eFuse only; NO Wi-Fi init */
    key_present=nvs_key_load();
    boot_session=esp_random();if(!boot_session)boot_session=1;
    commands=xQueueCreate(OG_EVENT_QUEUE,sizeof(og_command_t));
    if(!commands){puts("FATAL|COMMAND_QUEUE");abort();}
    offgrid_ble_init(own_mac,pair_key,key_present);
    if(xTaskCreate(radio_loop,"sx1278",8192,NULL,5,NULL)!=pdPASS)abort();
    if(key_present){og_command_t k={.type=CMD_KEY};memcpy(k.key,pair_key,32);queue_command(&k);}
    if(xTaskCreate(ble_commands,"ble_cmd",4096,NULL,4,NULL)!=pdPASS)abort();
    if(xTaskCreate(serial_loop,"usb_serial",4096,NULL,4,NULL)!=pdPASS)abort();
}
