package com.offgrid.messenger

import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.security.MessageDigest
import java.security.SecureRandom
import javax.crypto.Cipher
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.SecretKeySpec
import java.util.zip.CRC32

/** Android-native equivalent of the existing web protocol.js, lora_media.js and
 * lora_bulk.js. It deliberately contains no Android classes or UI code. */
object LoRaNativeProtocol {
    const val MAX_BULK = 7168
    const val MAX_PHOTO = 48 * 1024
    const val MAX_VOICE = 40 * 1024
    const val MEDIA_SEGMENT = 7040
    const val MEDIA_PART = 12
    const val RECEIPT_BATCH = 11
    private val random = SecureRandom()
    private val mimes = arrayOf("image/webp", "image/png", "image/jpeg", "audio/webm", "audio/ogg", "audio/mp4")

    data class Sealed(val transferId:String,val ciphertext:ByteArray)
    data class MediaPart(val kind:Int,val mime:String,val index:Int,val count:Int,val total:Int,
                         val sha16:ByteArray,val chunk:ByteArray)
    data class MediaResult(val kind:Int,val mime:String,val bytes:ByteArray,
                           val width:Int,val height:Int,val timestamp:Long)
    data class Receipt(val kind:Int,val id:String)

    fun seal(key:ByteArray,kind:Int,messageId:String,body:ByteArray,width:Int=0,height:Int=0,tsSeconds:Long=System.currentTimeMillis()/1000):Sealed {
        require(key.size==32 && kind in 1..MEDIA_PART && width in 0..65535 && height in 0..65535)
        require(body.size<=MAX_BULK-24-28)
        val msg=OgmCodec.unhex(messageId);require(msg.size==8)
        val transfer=ByteArray(8)
        do { random.nextBytes(transfer) }
        while (transfer.copyOfRange(0,4).all{it==0.toByte()} || transfer.copyOfRange(4,8).all{it==0.toByte()})
        val plain=ByteBuffer.allocate(24+body.size).order(ByteOrder.BIG_ENDIAN)
        plain.put('O'.code.toByte());plain.put('G'.code.toByte());plain.put(1);plain.put(kind.toByte());plain.put(msg)
        plain.putInt(tsSeconds.toInt());plain.putShort(width.toShort());plain.putShort(height.toShort())
        plain.putInt(body.size);plain.put(body)
        val iv=ByteArray(12).also(random::nextBytes)
        val cipher=Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.ENCRYPT_MODE,SecretKeySpec(key,"AES"),GCMParameterSpec(128,iv))
        cipher.updateAAD("OGM4".toByteArray(Charsets.US_ASCII)+transfer)
        val result=iv+cipher.doFinal(plain.array())
        require(result.size in 28..MAX_BULK)
        return Sealed(OgmCodec.hex(transfer),result)
    }
    fun open(key:ByteArray,transferId:String,ciphertext:ByteArray):OgmCodec.Envelope =
        OgmCodec.decrypt(key,transferId,ciphertext)
    fun crc32(bytes:ByteArray):String=CRC32().also{it.update(bytes)}.value.toString(16).uppercase().padStart(8,'0')

    fun mediaParts(kind:Int,mime:String,data:ByteArray):List<ByteArray>{
        require(kind==OgmCodec.IMAGE || kind==OgmCodec.VOICE)
        val valid=if(kind==OgmCodec.IMAGE)mimes.take(3) else mimes.drop(3)
        require(mime in valid && data.size in 256..(if(kind==OgmCodec.IMAGE)MAX_PHOTO else MAX_VOICE))
        val tag=valid.indexOf(mime)
        val hash=MessageDigest.getInstance("SHA-256").digest(data).copyOf(16)
        val count=(data.size+MEDIA_SEGMENT-1)/MEDIA_SEGMENT
        require(count in 1..10)
        return List(count){i->
            val chunk=data.copyOfRange(i*MEDIA_SEGMENT,minOf(data.size,(i+1)*MEDIA_SEGMENT))
            ByteBuffer.allocate(28+chunk.size).order(ByteOrder.BIG_ENDIAN).apply{
                put(1);put(kind.toByte());put(tag.toByte());put(0)
                putShort(i.toShort());putShort(count.toShort());putInt(data.size);put(hash);put(chunk)
            }.array()
        }
    }
    fun parseMedia(body:ByteArray):MediaPart?{
        if(body.size !in 29..(28+MEDIA_SEGMENT)||body[0].toInt()!=1||body[3].toInt()!=0)return null
        val kind=body[1].toInt() and 255
        if(kind!=OgmCodec.IMAGE && kind!=OgmCodec.VOICE)return null
        val idx=body[2].toInt() and 255;if(idx>2)return null
        val b=ByteBuffer.wrap(body).order(ByteOrder.BIG_ENDIAN);b.position(4)
        val part=b.short.toInt() and 65535;val count=b.short.toInt() and 65535;val total=b.int
        val limit=if(kind==OgmCodec.IMAGE)MAX_PHOTO else MAX_VOICE
        if(total !in 256..limit||count !in 1..10||count!=(total+MEDIA_SEGMENT-1)/MEDIA_SEGMENT||part>=count)return null
        if(body.size-28!=minOf(MEDIA_SEGMENT,total-part*MEDIA_SEGMENT))return null
        val hash=ByteArray(16);b.get(hash)
        val mime=mimes[if(kind==OgmCodec.IMAGE)idx else idx+3]
        return MediaPart(kind,mime,part,count,total,hash,body.copyOfRange(28,body.size))
    }
    class MediaReceiver {
        private data class Partial(val kind:Int,val mime:String,val count:Int,val total:Int,
            val hash:ByteArray,val width:Int,val height:Int,val timestamp:Long,
            val parts:Array<ByteArray?>,var updated:Long)
        private val pending=linkedMapOf<String,Partial>()
        fun clear()=pending.clear()
        fun accept(peer:String,id:String,body:ByteArray,width:Int,height:Int,timestamp:Long):MediaResult?{
            val part=parseMedia(body)?:throw IllegalArgumentException("Malformed media segment")
            val name="$peer|$id"
            val now=System.currentTimeMillis()
            pending.entries.removeAll{now-it.value.updated>20*60_000}
            var state=pending[name]
            if(state==null){
                if(pending.size>=3)pending.remove(pending.keys.first())
                state=Partial(part.kind,part.mime,part.count,part.total,part.sha16,width,height,timestamp,
                    arrayOfNulls(part.count),now)
                pending[name]=state
            }
            require(state.kind==part.kind && state.mime==part.mime && state.count==part.count &&
                state.total==part.total && state.hash.contentEquals(part.sha16) &&
                state.width==width && state.height==height && state.timestamp==timestamp) {"Conflicting media segment"}
            val old=state.parts[part.index]
            if(old==null)state.parts[part.index]=part.chunk
            else require(old.contentEquals(part.chunk)){"Conflicting duplicate media segment"}
            state.updated=now
            if(state.parts.any{it==null})return null
            val data=ByteArray(state.total);var offset=0
            state.parts.forEach { piece -> val p=piece!!;p.copyInto(data,offset);offset+=p.size }
            require(offset==data.size){"Media length mismatch"}
            val hash=MessageDigest.getInstance("SHA-256").digest(data).copyOf(16)
            if(!hash.contentEquals(state.hash)) {pending.remove(name);throw IllegalArgumentException("Media digest mismatch")}
            pending.remove(name)
            return MediaResult(part.kind,part.mime,data,width,height,timestamp)
        }
    }
    fun readReceiptBatch(body:ByteArray):List<Receipt>{
        require(body.size>=10 && (body[0].toInt() and 255) in 1..8)
        val count=body[0].toInt() and 255
        require(body.size==1+count*9)
        val seen=mutableSetOf<String>()
        return List(count){i->
            val kind=body[1+9*i].toInt() and 255
            require(kind==OgmCodec.DELIVERED||kind==OgmCodec.READ)
            val id=OgmCodec.hex(body.copyOfRange(2+9*i,10+9*i))
            require(seen.add("$kind:$id"))
            Receipt(kind,id)
        }
    }
}
