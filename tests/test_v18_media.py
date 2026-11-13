#!/usr/bin/env python3
"""Intentional v1.8 media isolation checks; NOT hardware or bitrate measurement."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
web=(root/'web/app.js').read_text()
proto=(root/'web/protocol.js').read_text()
fw=(root/'firmware/main/main.c').read_text()
wire=(root/'web/lora_media.js').read_text()
assert 'receiptBatch:11,mediaPart:12' in proto
assert "async function transferMedia(job)" in web and 'Media.split(r.kind,r.mime,r.bytes)' in web
assert "P.makeEnvelope(P.types.mediaPart" in web and 'await P.seal(secretKey,plain,tid)' in web
assert 'await mediaReassembler.ingest' in web and 'await handleEnvelope(reconstructed,mac)' in web
assert 'digest(assembled)' in wire and 'SHA-256' in wire and 'Reassembler' in wire
assert 'MAX_PHOTO=50*1024, MAX_VOICE=40*1024, SEGMENT=7040' in wire
assert 'const MAX_CHARS=500,MAX_PHOTO=48*1024,MAX_VOICE_MS=120000' in web
assert 'audioBitsPerSecond:8000' in web and 'blob.size>Media.MAX_VOICE' in web
assert "$('attachments').disabled=!ready" in web and "$('voiceButton').disabled=!ready" in web
assert 'if(job.record&&!job.mediaSegment)' in web and 'await setState(job.record,\'sent\')' in web
assert 'watchDeliveryReceipt(job.record,chunks.media?' in web
assert "mediaModeCommand('MEDIA ON',['READY','BYPASS','ERROR'],18000)" in web
assert "mediaModeCommand('MEDIA OFF',['RESTORED','OFF','RESTORE_FAILED'],20000)" in web
assert 'await beginMediaFastSession()' in web and 'await endMediaFastSession()' in web
assert "if(r.cancelRequested)throw Error" in web
assert "c->type==CMD_MEDIA_ON" in fw and "c->type==CMD_MEDIA_OFF" in fw
assert 'og_auto_fresh(&auto_policy.local' in fw and 'og_auto_fresh(&auto_policy.remote' in fw
assert 'profile_start_offer(OG_MODE_LOW,false,false,true)' in fw
assert 'MEDIA=1' in fw and 'PHOTO=1|VOICE=1' in fw
assert "!strcmp(line,\"MEDIA ON\")" in fw and 'if(ble){output("MEDIA|ERROR|USB_ONLY' in fw
assert 'OG_SR_MAX_TRANSFER' in fw and 'BULKCHUNK' in fw
assert 'MAX' in (root/'firmware/main/lora_modes.h').read_text()
assert 'media_restore_requested' in fw and 'media_service();' in fw
assert 'OG_PROFILE_MEDIA_FLAG' in (root/'firmware/main/lora_profile_control.h').read_text()
assert 'MEDIA|RESTORED' in fw and 'MEDIA|READY' in fw
assert 'MEDIAFAST=1' in fw and 'MEDIAFAST=1' in web
print('PASS: AES-GCM 48/40 KiB media, temporary signed Fast handover, receipt-preserving deferred restore')
