package com.offgrid.messenger

import java.io.ByteArrayOutputStream
import java.util.Base64

/** Host-testable host <-> ESP32 LoRa BLE adapter contracts. */
object LoRaMobileCodec {
    const val MAX_LINE=508
    const val MAX_WRITE=16384
    fun parsePairingKey(value:String):ByteArray {
        val s=value.trim()
        val key=when {
            Regex("[0-9a-fA-F]{64}").matches(s)->ByteArray(32){i->s.substring(i*2,i*2+2).toInt(16).toByte()}
            Regex("[A-Za-z0-9_-]{43}").matches(s)->Base64.getUrlDecoder().decode(s+"=")
            else->throw IllegalArgumentException("Expected 64 hex or 43 base64url characters")
        }
        require(key.size==32){"Pairing key must be 32 bytes"}
        return key
    }
    /** Require exact authenticated feature tokens, not partial substring matches. */
    fun supportsBleBulkCaps(line:String):Boolean {
        val fields=line.split('|').toSet()
        return line.startsWith("CAPS|") && setOf("FAMILY=LORA","PROTO=2","MEDIA=1","BULK=1")
            .all {it in fields}
    }
    class LineWriter {
        private val pending=ByteArrayOutputStream()
        fun append(bytes:ByteArray):List<String>{
            require(bytes.size<=MAX_WRITE){"Write exceeds native transfer boundary"}
            val complete=mutableListOf<String>()
            for(b in bytes){
                val value=b.toInt() and 255
                when {
                    value==10->{
                        val text=pending.toByteArray().toString(Charsets.US_ASCII).trimEnd('\r')
                        pending.reset()
                        if(text.isNotEmpty())complete.add(text)
                    }
                    value in 32..126 && pending.size()<MAX_LINE ->pending.write(value)
                    else -> {pending.reset();throw IllegalArgumentException("Invalid BLE ASCII record")}
                }
            }
            return complete
        }
        fun reset(){pending.reset()}
    }
}
