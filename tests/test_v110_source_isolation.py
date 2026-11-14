#!/usr/bin/env python3
"""v3.0.4 checks protected security logic rather than obsolete full-file hashes."""
from pathlib import Path
import subprocess
r=Path(__file__).resolve().parents[1]
for path in ('web/protocol.js','web/lora_media.js','web/lora_receipts.js',
             'web/lora_delivery.js','web/lora_rx_guard.js',
             'android/app/src/main/java/com/offgrid/messenger/OgmCodec.kt',
             'firmware/main/ble_auth_psa.h'):
 assert (r/path).read_bytes()==subprocess.check_output(['git','show','v3.0.3-rc.1-android-native-rf:'+path],cwd=r),path
assert 'AuthenticatedRxGuard' in (r/'web/lora_rx_guard.js').read_text()
assert 'AES/GCM/NoPadding' in (r/'android/app/src/main/java/com/offgrid/messenger/OgmCodec.kt').read_text()
print('PASS: protected AES-GCM, receipt and receive-guard code unchanged in v3.0.4')
