package com.offgrid.messenger

import android.content.Context

/** Uses the mature authenticated ESP-NOW-era GATT client against the LoRa GATT
 * service, with bulk records enabled by the opt-in v2.0 companion firmware.
 * A complete newline record remains the only BLE command unit. */
class LoRaBlePort(
    context:Context,
    private val deviceMac:String,
    private val keys:LoRaPairingKeys,
    private val onBytes:(ByteArray)->Unit,
    private val onLost:(String)->Unit
) {
    private val lines=LoRaMobileCodec.LineWriter()
    private var connected=false
    private var capable=false
    private val early=ArrayDeque<String>()
    private var closed=false
    private val ble:BleNodeClient by lazy { BleNodeClient(context,{ keys.load(deviceMac) },{ status,ready ->
        if(!closed && ready) {
            connected=true
            // The v1.8 BLE service deliberately rejected media. Do not report
            // the port open until the opt-in BLE-bulk firmware proves support.
            ble.send("CAPS?")
        }else if(!closed && connected) {
            connected=false
            onLost(status)
        }else if(!closed && (status.startsWith("Authentication failed")||status.startsWith("Pairing key"))) {
            onOpen?.invoke(false,status);onOpen=null
        }
    },{ response ->
        if(!capable && response.startsWith("CAPS|")) {
            if(LoRaMobileCodec.supportsBleBulkCaps(response)) {
                capable=true
                onOpen?.invoke(true,"");onOpen=null
                for(record in early)onBytes((record+"\n").toByteArray(Charsets.US_ASCII))
                early.clear()
            }else {
                onOpen?.invoke(false,"ESP32 BLE bulk media is not enabled. Flash the Android v2 companion firmware on both radios, or use USB OTG without reflashing.")
                onOpen=null;closed=true;ble.stop()
            }
        }else if(capable) {
            onBytes((response+"\n").toByteArray(Charsets.US_ASCII))
        }else if(early.size<24){
            early.addLast(response)
        }
    }) }
    private var onOpen:((Boolean,String)->Unit)?=null
    fun open(callback:(Boolean,String)->Unit){onOpen=callback;ble.connect(deviceMac)}
    fun write(bytes:ByteArray,done:(Boolean,String)->Unit){
        if(!connected||!capable||closed){done(false,"BLE bulk not authenticated/capable");return}
        // Native Web Serial writes may contain multiple whole ASCII commands.
        // Keep complete line boundaries for BleNodeClient's GATT scheduler.
        try {
            for(cmd in lines.append(bytes)) {
                if(cmd.startsWith("KEYGET")) {
                    // BLE firmware correctly forbids NVS key export; obtain
                    // only the key explicitly saved in AndroidKeyStore.
                    val key=keys.load(deviceMac) ?: throw IllegalStateException("BLE pairing key missing")
                    try{onBytes(("KEYDATA|"+OgmCodec.hex(key)+"\n").toByteArray(Charsets.US_ASCII))}
                    finally{key.fill(0)}
                }else if(cmd.startsWith("KEYSET ")) {
                    throw IllegalStateException("Provision or change the ESP32 key over physical USB")
                }else ble.send(cmd)
            }
            done(true,"")
        }catch(e:Exception){lines.reset();done(false,e.message?:"BLE transfer error")}
    }
    fun close(){closed=true;connected=false;capable=false;ble.stop();onOpen=null;early.clear();lines.reset()}
}
