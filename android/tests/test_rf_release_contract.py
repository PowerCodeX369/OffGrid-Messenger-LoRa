from pathlib import Path
root=Path(__file__).resolve().parents[2]
android=(root/'android/app/build.gradle.kts').read_text()
radio=(root/'android/app/src/main/java/com/offgrid/messenger/RadioLink.kt').read_text()
ui=(root/'android/app/src/main/java/com/offgrid/messenger/MainActivity.kt').read_text()
firmware=(root/'firmware/main/main.c').read_text()
policy=(root/'firmware/main/lora_auto.h').read_text()
assert 'versionCode=303' in android and '3.0.3-rc.1-native-lora-rf' in android
assert 'RF_POLICY=EARLY88' in firmware and 'PEER_AGE_MS=' in firmware and '|RFHEARD=' in firmware
assert 'QUALITY_READY=' in firmware and 'RF_POLICY=EARLY88' in (root/'firmware/main/ble_bridge.c').read_text()
assert 'PEER_AGE_MS' in radio and 'LoRa peer silent' in radio
assert 'Auto RF rejected by ESP32' in ui and 'LoRa profile change failed' in ui
assert 'OG_AUTO_FAST_DOWN_RSSI (-84)' in policy and 'OG_AUTO_BAL_DOWN_RSSI (-88)' in policy
assert '(!both_fresh || remote_weak' in firmware and 'local_weak ||' in firmware
print('PASS: early RF policy markers, native app version upgrade, stale-link warning, failure visibility, weak-media guard')
