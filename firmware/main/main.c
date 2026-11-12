/*
 * OffGrid Messenger LoRa v1.5.0-rc.1 / ESP32-C3 + SX1278 Ra-02
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
static const uint8_t KDF_CONTEXT[] = "OffGrid-LoRa-HMAC-v1";

typedef enum { CMD_DATA=1, CMD_URGENT, CMD_APPACK, CMD_READ, CMD_STATUS, CMD_KEY, CMD_PROFILE } og_cmd_type_t;
typedef struct {
    og_cmd_type_t type;
    uint8_t mac[6];
    uint32_t session, id;
    uint16_t length, ble_token;
    bool ble_binary;
    uint8_t profile_target;
    bool force_profile;
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
    for(size_t i=0;i<OG_PENDING;i++)if(pending[i].used)return true;
    return false;
}
static og_frame_t profile_frame(uint8_t type, uint8_t target, uint32_t session, uint32_t nonce) {
    og_frame_t f={0};f.magic=OG_MAGIC;f.version=OG_VERSION;f.type=type;
    memcpy(f.sender,own_mac,6);f.session=session;f.id=nonce;f.length=5;
    f.data[0]=target;
    for(int i=0;i<4;i++)f.data[i+1]=(uint8_t)(nonce>>(8*i));
    return f;
}
static bool profile_nonce_valid(const og_frame_t *f) {
    uint32_t n=0;
    for(int i=0;i<4;i++)n|=(uint32_t)f->data[i+1]<<(8*i);
    return f->length==5 && n==f->id && f->data[0]>=OG_MODE_LOW && f->data[0]<=OG_MODE_MAX;
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
    peer_seen=false;memset(peer_mac,0,6);
    memset(pending,0,sizeof(pending));memset(received,0,sizeof(received));
    memset(controls,0,sizeof(controls));
    profile_state.phase=OG_PROFILE_IDLE;
    profile_assist_until_ms=0;profile_assist_next_ms=0;
    last_profile_offer_valid=false;
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
    if(f->type==OG_TYPE_RADIO_ACK){
        for(size_t i=0;i<OG_CTRL_QUEUE;i++)if(
            controls[i].frame.magic && controls[i].frame.type==OG_TYPE_RADIO_ACK &&
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
    if(f->type==OG_TYPE_RADIO_ACK || f->type==OG_TYPE_PROFILE_AGREE){
        for(size_t i=0;i<OG_CTRL_QUEUE;i++)if(controls[i].frame.type==OG_TYPE_HELLO){
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
    esp_err_t e=offgrid_sx1278_send(sx,bytes,n+OG_AUTH_BYTES,OG_RADIO_TX_TIMEOUT_MS);
    if(e==ESP_OK)stat_tx++;
    else stat_tx_errors++;
    if(e!=ESP_OK)output("RADIO_DIAG|TX_ERROR|%s\n",esp_err_to_name(e));
    return e;
}
static void profile_report(const char *state) {
    offgrid_sx1278_profile_t p=og_mode_profile(profile_state.current);
    output("PROFILE|%s|%s|SF=%u|BW=%" PRIu32 "|TX_DBM=%d|AUTO=0\n",
           state,og_mode_name(profile_state.current),(unsigned)p.spreading_factor,
           (uint32_t)p.bandwidth,(int)p.tx_power_dbm);
}
static void profile_reset_pending(void) {
    /* Key or mode changes never fake delivered/read receipts. */
    for(size_t i=0;i<OG_PENDING;i++)if(pending[i].used){
        output("FAILED|%" PRIu32 "|%" PRIu32 "|PROFILE_CHANGED_UNCONFIRMED\n",
               pending[i].frame.session,pending[i].frame.id);
        pending[i].used=false;stat_fail++;
    }
    memset(controls,0,sizeof(controls));
}
static void profile_start_offer(uint8_t target, bool force) {
    if(!radio_key_ready||!sx){output("PROFILE|ERROR|RADIO_NOT_READY\n");return;}
    if(profile_state.phase!=OG_PROFILE_IDLE){output("PROFILE|ERROR|TRANSITION_IN_PROGRESS\n");return;}
    if(target==profile_state.current){profile_report("ACTIVE");return;}
    if(profile_has_pending()&&!force){output("PROFILE|ERROR|PENDING_MESSAGES_USE_FORCE\n");return;}
    if(!peer_seen||millis()-peer_at_ms>OG_PEER_STALE_MS){output("PROFILE|ERROR|NO_AUTHENTICATED_PEER\n");return;}
    if(force)profile_reset_pending();
    profile_state.previous=profile_state.current;
    profile_state.target=target;
    profile_state.phase=OG_PROFILE_OFFERING;
    profile_assist_until_ms=0;profile_assist_next_ms=0;
    profile_state.nonce=esp_random();if(!profile_state.nonce)profile_state.nonce=1;
    profile_state.offer_session=boot_session;
    profile_state.attempts=0;
    profile_state.retry_at_ms=0;
    output("PROFILE|NEGOTIATING|%s|MANUAL=1\n",og_mode_name(target));
}
static bool profile_apply(uint8_t mode) {
    offgrid_sx1278_profile_t p=og_mode_profile(mode);
    esp_err_t e=offgrid_sx1278_set_profile(sx,&p);
    if(e!=ESP_OK){output("PROFILE|ERROR|RADIO_CONFIG_%s\n",esp_err_to_name(e));return false;}
    profile_state.current=mode;
    /* New data can only flow once an authenticated HELLO proves both peers
     * really switched. Until then, hold normal outgoing transmissions. */
    return true;
}
static void profile_service(void) {
    const int64_t now=millis();
    if(profile_state.phase==OG_PROFILE_OFFERING && now>=profile_state.retry_at_ms){
        if(profile_state.attempts>=5){
            profile_state.phase=OG_PROFILE_IDLE;
            output("PROFILE|TIMEOUT|%s|PEER_DID_NOT_AGREE\n",og_mode_name(profile_state.target));
        }else{
            og_frame_t offer=profile_frame(OG_TYPE_PROFILE_OFFER,profile_state.target,
                                           profile_state.offer_session,profile_state.nonce);
            (void)tx(&offer);
            profile_state.attempts++;
            profile_state.retry_at_ms=millis()+OG_PROFILE_OFFER_RETRY_MS;
        }
    }
    if(profile_state.phase==OG_PROFILE_WAIT_SWITCH && now>=profile_state.switch_at_ms){
        const uint8_t old=profile_state.previous;
        if(!profile_apply(profile_state.target)){
            (void)profile_apply(old);
            profile_state.phase=OG_PROFILE_IDLE;
            profile_report("ROLLED_BACK");return;
        }
        profile_state.phase=OG_PROFILE_VERIFYING;
        profile_state.verify_deadline_ms=millis()+OG_PROFILE_VERIFY_MS;
        profile_state.probe_at_ms=0;
        output("PROFILE|VERIFYING|%s\n",og_mode_name(profile_state.current));
    }
    if(profile_state.phase==OG_PROFILE_VERIFYING){
        if(now>=profile_state.verify_deadline_ms){
            (void)profile_apply(profile_state.previous);
            profile_state.phase=OG_PROFILE_IDLE;stat_profile_rollbacks++;
            profile_assist_until_ms=0;profile_assist_next_ms=0;
            memset(controls,0,sizeof(controls));
            profile_report("ROLLED_BACK");
        }else if(now>=profile_state.probe_at_ms){
            /* Authenticated HELLOs let both nodes confirm the new RF profile.
             * No application traffic or benchmark traffic until verified. */
            og_frame_t hello=new_frame(OG_TYPE_HELLO,boot_session,0);
            (void)tx(&hello);
            profile_state.probe_at_ms=millis()+650+(esp_random()%250);
        }
    }
    if(profile_state.phase==OG_PROFILE_IDLE && profile_assist_until_ms>now &&
       now>=profile_assist_next_ms && !profile_has_pending()){
        og_frame_t hello=new_frame(OG_TYPE_HELLO,boot_session,0);
        (void)tx(&hello);
        profile_assist_next_ms=millis()+530+(esp_random()%220);
    }
}
static void show_ready(const char *source) {
    char mac[18];mac_text(own_mac,mac);
    output("READY|%s|CHANNEL=0|SESSION=%" PRIu32 "|PROTO=2|CAP=180|STABLE=1|NVSKEY=1|FAMILY=LORA|RADIO=SX1278|FW=1.5.0-rc.1|DIAG=1|SACK=0|ACKENGINE=2|FLOW=2|PROFILES=3|BENCH=APP|FROM=%s\n",mac,boot_session,source);
}
static void show_link(void) {
    char p[18];if(peer_seen)mac_text(peer_mac,p);else strcpy(p,"NONE");
    unsigned pending_count=0,ctrl_count=0;
    for(size_t i=0;i<OG_PENDING;i++)if(pending[i].used)pending_count++;
    for(size_t i=0;i<OG_CTRL_QUEUE;i++)if(controls[i].frame.magic)ctrl_count++;
    output("LINK|%s|RSSI=%d|PHY=0|MODE=LORA|OK=%" PRIu32 "|RETRY=%" PRIu32
           "|FAIL=%" PRIu32 "|TXOK=%" PRIu32 "|TXFAIL=%" PRIu32 "|RX=%" PRIu32
           "|HELLO=%" PRIu32 "|CHANGES=0|SNR=NA|RADIOACK=%" PRIu32
           "|BADTAG=%" PRIu32 "|CRC=%" PRIu32 "|TXDONE=%" PRIu32
           "|APPACK=%" PRIu32 "|CAD_BUSY=%" PRIu32 "|PENDING=%u|CTRLQ=%u|PROFILE=%s|PROFILE_PHASE=%d|PROFILE_CHANGES=%" PRIu32 "|PROFILE_ROLLBACKS=%" PRIu32 "\n",
           p,(int)peer_rssi,stat_delivered,stat_retry,stat_fail,stat_tx,stat_tx_errors,
           stat_rx,stat_hello,stat_radio_ack,stat_badtag,stat_crc,
           stat_tx_done,stat_appack,stat_busy_cad,
           pending_count,ctrl_count,og_mode_name(profile_state.current),
           (int)profile_state.phase,stat_profile_changes,stat_profile_rollbacks);
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
    memcpy(peer_mac,f->sender,6);peer_seen=true;peer_at_ms=t;peer_rssi=rssi;
    return true;
}
static void received_frame(const uint8_t *bytes,size_t n,int16_t rssi) {
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
    stat_rx++;
    /* A correct HMAC on the NEW profile is the final confirmation. */
    if(profile_state.phase==OG_PROFILE_VERIFYING){
        profile_state.phase=OG_PROFILE_IDLE;
        stat_profile_changes++;
        profile_assist_until_ms=millis()+3200;
        profile_assist_next_ms=millis()+120+(esp_random()%180);
        profile_report("ACTIVE");
    }
    if(f.type==OG_TYPE_HELLO){stat_hello++;return;}
    if(f.type==OG_TYPE_PROFILE_OFFER){
        if(!profile_nonce_valid(&f))return;
        if(profile_state.phase==OG_PROFILE_IDLE && !profile_has_pending()){
            if(last_profile_offer_valid && f.id==last_profile_offer_nonce &&
               f.session==last_profile_offer_session &&
               !memcmp(f.sender,last_profile_offer_mac,6))return;
            last_profile_offer_valid=true;
            last_profile_offer_nonce=f.id;
            last_profile_offer_session=f.session;
            memcpy(last_profile_offer_mac,f.sender,6);
            profile_state.previous=profile_state.current;
            profile_state.target=f.data[0];
            profile_state.nonce=f.id;profile_state.offer_session=f.session;
            profile_state.phase=OG_PROFILE_RESPONDING;
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
    if(f.type==OG_TYPE_PROFILE_AGREE){
        if(profile_state.phase==OG_PROFILE_OFFERING && profile_nonce_valid(&f) &&
           profile_state.nonce==f.id && profile_state.offer_session==f.session &&
           profile_state.target==f.data[0]){
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
    if(profile_state.phase!=OG_PROFILE_IDLE){output("ERROR|PROFILE_SWITCH_IN_PROGRESS\n");return;}
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
    if(c->type==CMD_STATUS){show_ready("STATUS");show_link();profile_report(profile_state.phase==OG_PROFILE_IDLE?"ACTIVE":"TRANSITION");return;}
    if(c->type==CMD_PROFILE){profile_start_offer(c->profile_target,c->force_profile);return;}
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
                f->type!=OG_TYPE_RADIO_ACK && f->type!=OG_TYPE_PROFILE_AGREE))continue;
            const int rank=(f->type==OG_TYPE_RADIO_ACK)?0:
                           (f->type==OG_TYPE_HELLO?2:1);
            if(rank!=priority || (waiting_for_ack && rank!=0))continue;
            /* Discovery cannot consume airtime while real peer traffic is
             * flowing. It is disposable, not a delivery guarantee. */
            if(rank==2 && peer_seen && now-peer_at_ms<2000){
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
    if(profile_state.phase!=OG_PROFILE_IDLE)return;
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
        uint8_t bytes[OG_AIR_MAX];size_t n=0;
        offgrid_sx1278_packet_info_t info={0};
        e=offgrid_sx1278_receive(sx,bytes,sizeof(bytes),&n,&info,OG_RX_POLL_MS);
        if(e==ESP_OK)received_frame(bytes,n,info.rssi_dbm);
        else if(e==ESP_ERR_INVALID_RESPONSE)stat_crc++;
        int64_t now=millis();
        profile_service();
        const int64_t hello_interval=peer_seen?OG_HELLO_MS:OG_DISCOVERY_HELLO_MS;
        if(profile_state.phase==OG_PROFILE_IDLE && now>=profile_assist_until_ms && radio_key_ready && now-last_hello_ms>=hello_interval){
            last_hello_ms=now+(esp_random()%450);
            bool busy=false;
            for(size_t i=0;i<OG_PENDING;i++)if(pending[i].used){busy=true;break;}
            if(peer_seen && now-peer_at_ms<2000)busy=true;
            if(!busy){
                og_frame_t hello=new_frame(OG_TYPE_HELLO,boot_session,0);
                (void)control_add(&hello,110+(esp_random()%300));
            }
        }
        if(now-last_link_ms>=OG_STATUS_MS){last_link_ms=now;show_link();}
        /* The USB READY beacon is for reconnect, not a source of continuous
         * serial pressure during sustained high-volume transfers. */
        if(now-last_ready_ms>=8000){last_ready_ms=now;show_ready("BEACON");}
        service_queues();
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
    if(!strcmp(line,"HELP")){output("HELP|MSGHEX,CTRLHEX,STATUS,PROFILE LOW/BALANCED/MAX [FORCE],KEYGET,KEYSET,KEYSTATUS,APPACK,READ,CAPS\n");return;}
    if(!strcmp(line,"CAPS") || !strcmp(line,"CAPS?")){
        output("CAPS|FAMILY=LORA|PROTO=2|SX1278=1|PROFILE=MANUAL3|TEXT=1|PHOTO=0|VOICE=0|WIFI=0|BLE=1\n");return;
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
    puts("BOOT|OffGrid Messenger LoRa v1.5.0-rc.1 (Wi-Fi and ESP-NOW excluded)");
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
