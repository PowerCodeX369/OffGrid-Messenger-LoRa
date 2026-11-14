package com.offgrid.messenger

import java.util.Base64

fun main(){
 val key=ByteArray(32){it.toByte()}
 val hex=key.joinToString(""){"%02x".format(it.toInt() and 255)}
 val url=Base64.getUrlEncoder().withoutPadding().encodeToString(key)
 check(LoRaMobileCodec.parsePairingKey(hex).contentEquals(key))
 check(LoRaMobileCodec.parsePairingKey(url).contentEquals(key))
 for(bad in listOf("", "0011", "not a key", "A".repeat(64),"0".repeat(65))) {
  if(bad=="A".repeat(64))continue // 64 A's are valid hex
  check(runCatching{LoRaMobileCodec.parsePairingKey(bad)}.isFailure)
 }
 val lines=LoRaMobileCodec.LineWriter()
 check(lines.append("BULKBEGIN ABC 2048 DATA\nBULKCH".toByteArray())==listOf("BULKBEGIN ABC 2048 DATA"))
 check(lines.append("UNK ABC 0 0011\nSTATUS\n".toByteArray())==listOf("BULKCHUNK ABC 0 0011","STATUS"))
 check(runCatching{lines.append("A".repeat(510).toByteArray())}.isFailure)
 lines.reset()
 check(runCatching{lines.append(byteArrayOf(0,10))}.isFailure)
 check(runCatching{lines.append(ByteArray(16385))}.isFailure)
 println("PASS: Kotlin Android BLE record boundaries and 256-bit key import")
}
