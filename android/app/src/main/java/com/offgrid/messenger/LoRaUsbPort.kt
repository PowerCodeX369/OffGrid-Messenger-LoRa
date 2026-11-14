package com.offgrid.messenger

import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.hardware.usb.UsbConstants
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbDeviceConnection
import android.hardware.usb.UsbEndpoint
import android.hardware.usb.UsbInterface
import android.hardware.usb.UsbManager
import android.os.Build
import java.io.IOException
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean

/** ESP32-C3 native USB Serial/JTAG CDC (Espressif 0x303a).
 * The 1.8.0-rc.4 USB line protocol is not changed. Does not claim support for
 * CP210x/CH340 adapters without their separate serial-driver initialization. */
class LoRaUsbPort(
    private val context: Context,
    private val onBytes: (ByteArray)->Unit,
    private val onLost: (String)->Unit
) {
    companion object {
        const val ACTION_PERMISSION = "com.offgrid.messenger.LORA_USB_PERMISSION"
        @Volatile private var waiting: Pair<LoRaUsbPort, (Boolean)->Unit>? = null
        fun devices(context: Context): List<UsbDevice> {
            val manager = context.getSystemService(Context.USB_SERVICE) as UsbManager
            return manager.deviceList.values.filter { it.vendorId == 0x303a }
        }
        fun permissionResult(intent: Intent) {
            val callback=waiting ?: return
            waiting=null
            val ok=intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED,false)
            callback.second(ok)
        }
    }
    private val io = Executors.newSingleThreadExecutor()
    private val readIo = Executors.newSingleThreadExecutor()
    private val reading=AtomicBoolean(false)
    private var connection: UsbDeviceConnection?=null
    private var usbInterface:UsbInterface?=null
    private var inEndpoint:UsbEndpoint?=null
    private var outEndpoint:UsbEndpoint?=null
    private var device:UsbDevice?=null

    fun open(selected:UsbDevice, onOpen:(Boolean,String)->Unit) {
        val manager=context.getSystemService(Context.USB_SERVICE) as UsbManager
        device=selected
        fun attach() {
            io.execute {
                try {
                    val port=manager.openDevice(selected) ?: throw IOException("USB device permission denied")
                    var match:Triple<UsbInterface,UsbEndpoint,UsbEndpoint>?=null
                    for (i in 0 until selected.interfaceCount) {
                        val iface=selected.getInterface(i)
                        val inp=(0 until iface.endpointCount).map {iface.getEndpoint(it)}
                            .firstOrNull { it.type==UsbConstants.USB_ENDPOINT_XFER_BULK && it.direction==UsbConstants.USB_DIR_IN }
                        val out=(0 until iface.endpointCount).map {iface.getEndpoint(it)}
                            .firstOrNull { it.type==UsbConstants.USB_ENDPOINT_XFER_BULK && it.direction==UsbConstants.USB_DIR_OUT }
                        if(inp!=null && out!=null) {match=Triple(iface,inp,out);break}
                    }
                    val (iface,inp,out)=match ?: throw IOException("ESP32 USB CDC bulk endpoints not found")
                    if(!port.claimInterface(iface,true))throw IOException("Could not claim USB interface")
                    // ESP32-C3 USB Serial/JTAG normally speaks CDC ACM, needing DTR+RTS.
                    // On a vendor-class interface the SET_CONTROL_LINE_STATE is ignored.
                    port.controlTransfer(0x21,0x22,0x03,0,ByteArray(0),0,1000)
                    connection=port;usbInterface=iface;inEndpoint=inp;outEndpoint=out
                    reading.set(true)
                    onOpen(true,"")
                    readIo.execute {
                        try {
                            val buf=ByteArray(2048)
                            while(reading.get()) {
                                val n=port.bulkTransfer(inp,buf,buf.size,350)
                                if(n>0)onBytes(buf.copyOf(n))
                                else if(selected.deviceName !in manager.deviceList.keys)
                                    throw IOException("ESP32 USB cable disconnected")
                            }
                        }catch(t:Throwable){if(reading.get())onLost("USB disconnected: ${t.message ?: "I/O error"}")}
                        finally{reading.set(false);closeConnection()}
                    }
                }catch(t:Throwable){
                    onOpen(false,t.message ?: "USB unavailable")
                }
            }
        }
        if(manager.hasPermission(selected))attach()
        else {
            waiting=Pair(this){ ok -> if(ok)attach() else onOpen(false,"USB permission was denied") }
            val intent=Intent(context,LoRaUsbPermissionReceiver::class.java).setAction(ACTION_PERMISSION)
            val flags=PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_MUTABLE
            manager.requestPermission(selected,PendingIntent.getBroadcast(context,7182,intent,flags))
        }
    }
    fun write(data:ByteArray, done:(Boolean,String)->Unit) {
        io.execute {
            val port=connection;val out=outEndpoint
            if(port==null||out==null) {done(false,"USB not connected");return@execute}
            try {
                var pos=0
                while(pos<data.size) {
                    val part=data.copyOfRange(pos, minOf(data.size,pos+512))
                    val n=port.bulkTransfer(out,part,part.size,2500)
                    if(n<=0)throw IOException("USB write timed out")
                    pos+=n
                }
                done(true,"")
            }catch(t:Throwable){done(false,t.message?:"USB write error")}
        }
    }
    fun close(){reading.set(false);closeConnection();io.shutdownNow();readIo.shutdownNow()}
    private fun closeConnection(){
        val port=connection;connection=null
        try{usbInterface?.let{port?.releaseInterface(it)}}catch(_:Exception){}
        try{port?.close()}catch(_:Exception){}
        usbInterface=null;inEndpoint=null;outEndpoint=null
    }
}

class LoRaUsbPermissionReceiver:BroadcastReceiver() {
    override fun onReceive(context:Context,intent:Intent) {
        if(intent.action==LoRaUsbPort.ACTION_PERMISSION)LoRaUsbPort.permissionResult(intent)
    }
}
