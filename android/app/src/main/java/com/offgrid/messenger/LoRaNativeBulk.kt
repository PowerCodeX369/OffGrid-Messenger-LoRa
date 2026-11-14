package com.offgrid.messenger

/** Event-driven BLE ASCII implementation of web/lora_bulk.js. The BLE client
 * owns ATT write serialization; this layer owns bounded bulk checkpoints.
 * Never reports radio delivery until an actual BULKCONFIRMED event. */
class LoRaNativeBulk(
    private val write:(String)->Unit,
    private val schedule:(Runnable,Long)->Unit,
    private val cancel:(Runnable)->Unit,
    private val completed:(Boolean,String)->Unit,
    private val transmitted:()->Unit,
    private val progress:(Int,Int)->Unit,
    private val received:(String,String,ByteArray)->Unit
){
    private data class Tx(val id:String,val data:ByteArray,val ctrl:Boolean,var state:String="begin",var seq:Int=0,var sent:Boolean=false)
    private data class Rx(val id:String,val mac:String,val bytes:ByteArray,var seq:Int=0,var offset:Int=0)
    private var tx:Tx?=null
    private var rx:Rx?=null
    private var timer:Runnable?=null
    private val batch=4;private val chunk=160
    val busy:Boolean get()=tx!=null
    fun reset(){timer?.let(cancel);timer=null;tx=null;rx=null}
    private fun watch(delay:Long){
        timer?.let(cancel)
        val a=Runnable { if(tx!=null)finish(false,"LoRa BLE bulk timeout") }
        timer=a;schedule(a,delay)
    }
    private fun finish(ok:Boolean,reason:String){
        timer?.let(cancel);timer=null
        tx=null;completed(ok,reason)
    }
    fun start(sealed:LoRaNativeProtocol.Sealed,control:Boolean){
        require(tx==null && sealed.ciphertext.size in 28..LoRaNativeProtocol.MAX_BULK)
        require(sealed.transferId.matches(Regex("[0-9A-F]{16}")) &&
          sealed.transferId.substring(0,8)!="00000000" && sealed.transferId.substring(8)!="00000000")
        tx=Tx(sealed.transferId,sealed.ciphertext,control)
        write("BULKBEGIN ${sealed.transferId} ${sealed.ciphertext.size} ${if(control)"CTRL" else "DATA"}")
        watch(12_000)
    }
    private fun sendBatch(t:Tx){
        val end=minOf(t.data.size,(t.seq+batch)*chunk)
        val checkpoint=(end+chunk-1)/chunk-1
        val messages=ArrayList<String>()
        while(t.seq*chunk<t.data.size && t.seq<=checkpoint){
            val begin=t.seq*chunk
            messages.add("BULKCHUNK ${t.id} ${t.seq} ${OgmCodec.hex(t.data.copyOfRange(begin,minOf(begin+chunk,t.data.size)))}")
            t.seq++
        }
        t.state="chunk:$checkpoint"
        // BLE writes are serialized by the existing original GATT client.
        messages.forEach(write)
        watch(16_000)
    }
    fun onLine(line:String):Boolean {
        val p=line.split('|');val type=p[0]
        if(type.startsWith("RXB")){receiveLine(p);return true}
        if(type !in setOf("BULKREADY","BULKSTEP","BULKQUEUED","BULKSENT","BULKPROGRESS","BULKCONFIRMED","BULKFAILED","BULKSTAT"))return false
        val t=tx?:return true
        if(p.getOrNull(1)!=t.id && p.getOrNull(1)!="SESSION")return true
        if(type=="BULKFAILED") {finish(false,line);return true}
        when(type){
            "BULKREADY"->if(t.state=="begin")sendBatch(t)
            "BULKSTEP"-> {
                val wanted=t.state.removePrefix("chunk:").toIntOrNull()
                if(wanted!=null && p.getOrNull(2)?.toIntOrNull()==wanted){
                    if(t.seq*chunk<t.data.size)sendBatch(t)
                    else{
                        t.state="end";write("BULKEND ${t.id} ${LoRaNativeProtocol.crc32(t.data)}");watch(12_000)
                    }
                }
            }
            "BULKQUEUED"->if(t.state=="end"){
                t.state="radio";watch(22_000+((t.data.size+223)/224)*4_500L)
            }
            "BULKSENT"->{if(!t.sent){t.sent=true;transmitted()}}
            "BULKPROGRESS"->{val n=p.getOrNull(2)?.toIntOrNull();val count=p.getOrNull(3)?.toIntOrNull();if(n!=null&&count!=null&&count>0)progress(n,count)}
            "BULKCONFIRMED"->if(t.state=="radio")finish(true,"")
        }
        return true
    }
    private fun receiveLine(p:List<String>){
        val type=p[0];val id=p.getOrNull(1)?:return
        if(!id.matches(Regex("[0-9A-Fa-f]{16}"))){rx=null;return}
        try{
            when(type){
                "RXBSTART"->{
                    val n=p.getOrNull(2)?.toIntOrNull()?:throw IllegalArgumentException("RX size")
                    require(n in 28..LoRaNativeProtocol.MAX_BULK)
                    val mac=p.getOrNull(3)?:throw IllegalArgumentException("Peer identity")
                    require(mac.matches(Regex("[0-9A-Fa-f:]{17}")))
                    rx=Rx(id.uppercase(),mac,ByteArray(n))
                }
                "RXBCHUNK"->{
                    val r=rx?:return
                    val seq=p.getOrNull(2)?.toIntOrNull()
                    require(r.id==id.uppercase() && seq==r.seq)
                    val b=OgmCodec.unhex(p.getOrNull(3)?:throw IllegalArgumentException("RX hex"))
                    require(b.size in 1..chunk && r.offset+b.size<=r.bytes.size)
                    b.copyInto(r.bytes,r.offset);r.offset+=b.size;r.seq++
                }
                "RXBDONE"->{
                    val r=rx?:return;rx=null
                    require(r.id==id.uppercase() && r.offset==r.bytes.size &&
                        LoRaNativeProtocol.crc32(r.bytes)==p.getOrNull(2)?.uppercase())
                    received(r.mac,r.id,r.bytes)
                }
            }
        }catch(_:IllegalArgumentException){rx=null}
    }
}
