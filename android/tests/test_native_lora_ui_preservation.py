from pathlib import Path
import subprocess, hashlib
root=Path(__file__).resolve().parents[2]
src=root/'android/app/src/main/java/com/offgrid/messenger'
original=subprocess.check_output(['git','show','v3.0.0-rc.1-android:android/app/src/main/java/com/offgrid/messenger/PremiumUi.kt'],cwd=root)
# Only the explicitly requested LoRa mode menu and its small mode-label text
# may differ from the original native screen. Every other Compose UI byte stays.
expected=original.decode()
expected=expected.replace('Text("ESP-NOW link",modifier=Modifier.padding(horizontal=16.dp,vertical=8.dp),',
                          'Text("SX1278 LoRa link",modifier=Modifier.padding(horizontal=16.dp,vertical=8.dp),')
expected=expected.replace('Text("Mode: ${radioLink?.rateMode ?: "unknown"} · Peer: ${radioLink?.peer ?: "unknown"}",',
                          'Text("Mode: ${radioLink?.modeLabel ?: "unknown"} · Peer: ${radioLink?.peer ?: "unknown"}",')
expected=expected.replace('''"AUTO"->"Auto (adaptive)";"SPEED"->"Speed";"RANGE"->"Range"
                                "LR500"->"True LR 500 kbps";else->"True LR 250 kbps"''',
                          '''"AUTO"->"Auto (adaptive)";"FAST"->"Fast · SF7 / 500 kHz"
                                "BALANCED"->"Balanced · SF7 / 250 kHz";else->"Max Range · SF9 / 125 kHz"''')
assert expected.encode()==(src/'PremiumUi.kt').read_bytes(), 'Changes outside the requested LoRa mode menu are forbidden'
assert not (src/'LoRaActivity.kt').exists(), 'New WebView UI must not exist'
assert not (root/'android/app/src/main/assets/www').exists(), 'Bundled second UI must be removed'
manifest=(root/'android/app/src/main/AndroidManifest.xml').read_text()
assert '<activity android:name=".MainActivity" android:exported="true"' in manifest
assert '<activity android:name=".LoRaActivity"' not in manifest
assert 'android.intent.action.MAIN' in manifest
assert b'LoRaNativeBulk' in (src/'MainActivity.kt').read_bytes()
assert 'BULKBEGIN' in (src/'LoRaNativeBulk.kt').read_text()
assert 'BULKCHUNK' in (src/'LoRaNativeBulk.kt').read_text()
assert 'RXBCHUNK' in (src/'LoRaNativeBulk.kt').read_text()
assert subprocess.check_output(['git','diff','--name-only','v3.0.0-rc.1-android','--','web','firmware/main','firmware/components'],cwd=root)==b'', 'Protected web or RF firmware changed'
print('PASS: Original native UI preserved except requested LoRa radio menu; native launcher, web and firmware unchanged')
