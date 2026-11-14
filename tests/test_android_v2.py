#!/usr/bin/env python3
"""Android v2.0 source isolation and explicit parity contracts; not an APK build."""
from pathlib import Path
import hashlib,re
r=Path(__file__).resolve().parents[1]
assets=r/'android/app/src/main/assets/www'
web=r/'web'
for file in web.iterdir():
    if file.is_file() and file.suffix in ('.html','.css','.js','.png','.webp','.svg'):
        copy=assets/file.name
        assert copy.is_file(),file.name
        a=file.read_bytes();b=copy.read_bytes()
        if file.name=='index.html':
            b=b.replace(b'  <script src="android_serial.js?v=android2rc1"></script>\n',b'')
        assert hashlib.sha256(a).digest()==hashlib.sha256(b).digest(),file.name
index=(assets/'index.html').read_text()
assert index.index('android_serial.js')<index.index('protocol.js')
shim=(assets/'android_serial.js').read_text()
for token in ['navigator,\'serial\'','ReadableStream','WritableStream','OffGridNative','AndroidSerialPort','savePhoto','Notification']:
    assert token in shim,token
activity=(r/'android/app/src/main/java/com/offgrid/messenger/LoRaActivity.kt').read_text()
for feature in ['WebViewAssetLoader','WebViewCompat.addWebMessageListener','WEB_MESSAGE_LISTENER',
                'onPermissionRequest','onShowFileChooser','LoRaUsbPort','LoRaBlePort',
                'Manifest.permission.RECORD_AUDIO','scanBle','notificationPermissionLauncher','photoSave']:
    assert feature in activity,feature
assert 'https://appassets.androidplatform.net' in activity
assert 'setAllowFileAccessFromFileURLs(true)' not in activity
assert 'setAllowUniversalAccessFromFileURLs(true)' not in activity
assert 'web.settings.allowFileAccess=false' in activity
assert not re.search(r'\bINTERNET\b',(r/'android/app/src/main/AndroidManifest.xml').read_text())
bridge=(r/'firmware/main/ble_bridge.c').read_text()
policy=(r/'firmware/main/ble_command_policy.h').read_text()
main=(r/'firmware/main/main.c').read_text()
assert 'if(!auth){push("AUTH_REQUIRED"' in bridge
assert '!strcmp(s,"KEYGET")' not in policy and '!strncmp(s,"KEYSET ",7)' not in policy
assert 'offgrid_ble_publish(line);' in main
assert 'BLE_DENIED' in bridge
assert 'POWER18|ERROR|PHYSICAL_USB_ONLY' not in main
assert 'ble_token' in main
# No original ESP-NOW Kotlin source was changed.
for line in (r/'docs/ANDROID_FROZEN_SHA256.txt').read_text().splitlines():
    if not line or line.startswith('#'):continue
    digest,rel=line.split('  ',1)
    if rel in ('README.md','app/build.gradle.kts','app/src/main/AndroidManifest.xml'):continue
    assert hashlib.sha256((r/'android'/rel).read_bytes()).hexdigest()==digest,rel
print('PASS: v1.10 browser parity, offline origin, USB/BLE capability, preserved ESP-NOW client')
