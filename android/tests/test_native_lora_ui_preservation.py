from pathlib import Path
import subprocess, hashlib
root=Path(__file__).resolve().parents[2]
src=root/'android/app/src/main/java/com/offgrid/messenger'
original=subprocess.check_output(['git','show','v3.0.0-rc.1-android:android/app/src/main/java/com/offgrid/messenger/PremiumUi.kt'],cwd=root)
assert original==(src/'PremiumUi.kt').read_bytes(), 'Original premium Compose UI was modified'
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
print('PASS: Original native UI byte-identical, native launcher restored, WebView removed; web and firmware untouched')
