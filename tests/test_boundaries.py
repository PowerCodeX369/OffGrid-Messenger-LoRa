#!/usr/bin/env python3
"""Source-structure checks. These are NOT ESP-IDF builds or RF tests."""
from pathlib import Path
import hashlib
import re
root = Path(__file__).resolve().parents[1]
main = (root/'firmware/main/main.c').read_text()
cmake = '\n'.join(line for line in (root/'firmware/main/CMakeLists.txt').read_text().splitlines() if not line.lstrip().startswith('#'))
web = (root/'web/app.js').read_text()
ui = (root/'web/index.html').read_text()
ble = (root/'firmware/main/ble_bridge.c').read_text()
for pat in ['esp_wifi_init', 'esp_wifi_start', 'esp_now_init', 'esp_now_send',
            'esp_now_register', 'esp_netif_init', 'esp_event_loop_create_default']:
    assert not re.search(r'\b'+pat+r'\s*\(', main), f'unexpected Wi-Fi/ESP-NOW call: {pat}'
for dep in ['esp_wifi', 'esp_now', 'esp_netif', 'esp_event']:
    assert not re.search(r'\b'+dep+r'\b', cmake), f'forbidden CMake dependency: {dep}'
assert 'offgrid_sx1278_open' in main and 'offgrid_sx1278_receive' in main
assert 'nimble_port_init' in ble and 'offgrid_ble_init' in main
assert 'FAMILY=LORA' in main and 'FAMILY=LORA' in web
assert 'OG_TYPE_RADIO_ACK' in main and 'OG_TYPE_DELIVERED' in main
assert 'OG_TYPE_READ' in main and 'PSA_ALG_HMAC(PSA_ALG_SHA_256)' in main
assert 'ESP-NOW</span>' not in ui and 'ESP-NOW channel 6' not in web
assert "writeLine('RATE '" not in web
assert "$('attachments').disabled=true" in web and "$('voiceButton').disabled=true" in web
assert 'urgentReceipts' in web and 'runUrgentReceipts' in web
assert 'FragmentScheduler' in web and 'rfScheduler.submit' in web
assert 'FLOW=2' in main and 'FLOW=2' in web
# rc.2 rolls back the rc.1 pre-TX CAD gate, preserving the v1.1 TX path.
service=main[main.index('static void service_queues(void)'):main.index('static void radio_loop(void *arg)')]
assert 'offgrid_sx1278_cad(' not in service and 'offgrid_sx1278_rx_busy(' not in service
assert 'esp_err_t status=tx(&p->frame);' in service
assert "P.types.probe,P.types.probeAck" in web
assert 'P.types.receiptBatch' in web and 'flushReceiptBatch' in web
assert 'LoRa speed test' in web
assert 'BINCAP|0|0' in ble and 'CAPS|FAMILY=LORA' in ble
assert 'OG_TX_ATTEMPTS' in main and 'RADIOACK|' in main
assert 'CTRLHEX ' in main and 'CMD_URGENT' in main
assert 'OG_APP_WAIT_MS' not in main and 'OG_TYPE_RECEIPT_QUERY' not in main
assert 'offgrid_sx1278_send' in main and 'radio_hmac' in main
assert 'static void service_queues' in main and 'choose_pending' in main
assert 'fifo_burst' in (root/'firmware/components/offgrid_sx1278/offgrid_sx1278.c').read_text()
assert 'SPI_DMA_CH_AUTO' in (root/'firmware/components/offgrid_sx1278/offgrid_sx1278.c').read_text()
assert 'OffGridLoRaDelivery' in web and 'RADIOACK' in web
assert 'CTRLHEX ' in web and 'watchDeliveryReceipt' in web
assert "1.2.0-rc.2" in main and "1.2.0-lora" in web
# Source copied to Android is truly unchanged against the imported ESP-NOW
# repository. The pinned reference hash list is checked during packaging.
assert (root/'docs/ANDROID_FROZEN_SHA256.txt').exists()
for line in (root/'docs/ANDROID_FROZEN_SHA256.txt').read_text().splitlines():
    if not line or line.startswith('#'): continue
    digest,path=line.split('  ',1)
    actual=hashlib.sha256((root/'android'/path).read_bytes()).hexdigest()
    assert digest==actual, f'Android source changed: {path}'
print('PASS: no Wi-Fi runtime, isolated LoRa, BLE/capability boundaries, no Android changes')
