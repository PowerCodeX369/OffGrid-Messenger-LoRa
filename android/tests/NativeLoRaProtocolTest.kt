package com.offgrid.messenger

import java.util.ArrayDeque
import java.security.SecureRandom

private fun checkEq(value:Any,expected:Any){check(value==expected){"$value != $expected"}}
fun main(){
    val key=ByteArray(32){it.toByte()}
    val secretId=OgmCodec.newId()
    val plain="native Android LoRa encrypted text ✔".toByteArray(Charsets.UTF_8)
    val text=LoRaNativeProtocol.seal(key,OgmCodec.TEXT,secretId,plain)
    checkEq(OgmCodec.decrypt(key,text.transferId,text.ciphertext).text.toList(),plain.toList())
    checkEq(OgmCodec.decrypt(key,text.transferId,text.ciphertext).id,secretId)
    val tampered=text.ciphertext.clone();tampered[tampered.size-1]=(tampered.last().toInt() xor 1).toByte()
    check(runCatching {OgmCodec.decrypt(key,text.transferId,tampered)}.isFailure)
    check(runCatching {OgmCodec.decrypt(ByteArray(32){0},text.transferId,text.ciphertext)}.isFailure)
    println("PASS: Native Android AES-GCM OGM4 text, randomized transfer nonce, tamper/wrong-key reject")

    for((kind,mime,size,expected) in listOf(
        listOf(OgmCodec.IMAGE,"image/jpeg",48*1024,7),
        listOf(OgmCodec.VOICE,"audio/ogg",40*1024,6)
    )){
        val k=kind as Int;val name=mime as String;val n=size as Int;val count=expected as Int
        val data=ByteArray(n);SecureRandom().nextBytes(data)
        val parts=LoRaNativeProtocol.mediaParts(k,name,data)
        checkEq(parts.size,count)
        val receiver=LoRaNativeProtocol.MediaReceiver()
        val mid=OgmCodec.newId()
        val ts=System.currentTimeMillis()/1000
        val dims=if(k==OgmCodec.IMAGE) Pair(1024,768) else MediaPayload.durationParts(120000)
        val encrypted=parts.map{LoRaNativeProtocol.seal(key,OgmCodec.MEDIA_PART,mid,it,dims.first,dims.second,ts)}
        check(encrypted.all{it.ciphertext.size<=7168})
        var got:LoRaNativeProtocol.MediaResult?=null
        for(i in encrypted.indices.reversed()){
            val e=encrypted[i]
            val open=OgmCodec.decrypt(key,e.transferId,e.ciphertext)
            checkEq(open.kind,OgmCodec.MEDIA_PART)
            got=receiver.accept("AA:BB:CC:DD:EE:FF",open.id,open.text,open.width,open.height,open.timestamp)?:got
        }
        check(got!=null && got.bytes.contentEquals(data) && got.mime==name)
        val duplicated=parts[0]
        check(receiver.accept("AA:BB:CC:DD:EE:FF",mid,duplicated,dims.first,dims.second,ts*1000)==null)
        check(runCatching{LoRaNativeProtocol.mediaParts(k,name,data+ByteArray(5*1024))}.isFailure)
        println("PASS: $name ${n/1024} KiB -> $count authenticated LoRa bulk segments, reverse reassembly, SHA16, limits")
    }
    val batch=byteArrayOf(2,3)+OgmCodec.unhex("0011223344556677")+byteArrayOf(4)+OgmCodec.unhex("8899AABBCCDDEEFF")
    val receipts=LoRaNativeProtocol.readReceiptBatch(batch)
    checkEq(receipts[0],LoRaNativeProtocol.Receipt(3,"0011223344556677"))
    checkEq(receipts[1],LoRaNativeProtocol.Receipt(4,"8899AABBCCDDEEFF"))
    check(runCatching{LoRaNativeProtocol.readReceiptBatch(batch+byteArrayOf(1))}.isFailure)
    println("PASS: Web-compatible encrypted receipt batch and strict rejection")

    val cmds=ArrayDeque<String>();val completed=mutableListOf<Boolean>();val observations=mutableListOf<String>()
    val alarms=mutableMapOf<Runnable,Long>()
    val bridge=LoRaNativeBulk({cmds.addLast(it)}, {r,ms-> alarms[r]=ms}, {r->alarms.remove(r)},
        {ok,_ ->completed.add(ok)},{observations.add("TX")},{a,b->observations.add("$a/$b")},
        {_,_,cipher->observations.add("RX:${cipher.size}")})
    bridge.start(text,false)
    val receivedData=ArrayList<Byte>()
    var stage=""
    var count=0
    while(cmds.isNotEmpty() && count++<200){
        val line=cmds.removeFirst();val words=line.split(' ')
        when(words[0]){
            "BULKBEGIN"->{stage=words[1];checkEq(words[2].toInt(),text.ciphertext.size);bridge.onLine("BULKREADY|$stage")}
            "BULKCHUNK"->{
                checkEq(words[1],stage)
                val fragment=OgmCodec.unhex(words[3]);receivedData.addAll(fragment.toList())
                val seq=words[2].toInt()
                if((seq+1)%4==0||receivedData.size==text.ciphertext.size)
                    bridge.onLine("BULKSTEP|$stage|$seq")
            }
            "BULKEND"->{
                checkEq(words[2],LoRaNativeProtocol.crc32(receivedData.toByteArray()))
                bridge.onLine("BULKQUEUED|$stage");bridge.onLine("BULKSENT|$stage")
                bridge.onLine("BULKCONFIRMED|$stage")
            }
            else -> error("Unexpected BLE bulk command $line")
        }
    }
    check(completed==listOf(true) && !bridge.busy && receivedData.toByteArray().contentEquals(text.ciphertext))
    check(observations.contains("TX"))
    // Recreate an inbound whole-ciphertext RXB stream; CRC is local, GCM authenticates it.
    bridge.onLine("RXBSTART|${text.transferId}|${text.ciphertext.size}|AA:BB:CC:DD:EE:FF")
    text.ciphertext.toList().chunked(160).forEachIndexed{ i,bytes->
        bridge.onLine("RXBCHUNK|${text.transferId}|$i|${OgmCodec.hex(bytes.toByteArray())}")
    }
    bridge.onLine("RXBDONE|${text.transferId}|${LoRaNativeProtocol.crc32(text.ciphertext)}")
    check(observations.any{it=="RX:${text.ciphertext.size}"})
    checkEq(LoRaNativeProtocol.crc32("123456789".toByteArray()),"CBF43926")
    println("PASS: Original BLE GATT client + LoRa bulk stage/bitmap status, TX/RX checkpoints, CRC32")
}
