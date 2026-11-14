package com.offgrid.messenger

import android.Manifest
import android.annotation.SuppressLint
import android.app.AlertDialog
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.ClipData
import android.content.ClipboardManager
import androidx.core.app.NotificationCompat
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.provider.MediaStore
import android.text.InputType
import android.util.Base64
import android.view.ViewGroup
import android.webkit.PermissionRequest
import android.webkit.ValueCallback
import android.webkit.WebChromeClient
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.EditText
import android.widget.FrameLayout
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.content.ContextCompat
import androidx.webkit.WebViewAssetLoader
import androidx.webkit.WebViewCompat
import androidx.webkit.WebViewFeature
import org.json.JSONObject
import java.io.ByteArrayInputStream
import java.util.ArrayDeque

/** Native Android shell for the exact v1.10 LoRa web UI and AES-GCM protocol.
 * Offline WebView origin + origin-scoped native message channel, not file://.
 * The previously released native ESP-NOW MainActivity remains available. */
@SuppressLint("SetJavaScriptEnabled", "MissingPermission")
class LoRaActivity:ComponentActivity() {
    companion object {
        private const val ORIGIN="https://appassets.androidplatform.net"
        private const val START_URL="$ORIGIN/assets/www/index.html"
    }
    private val main=Handler(Looper.getMainLooper())
    private lateinit var web:WebView
    private lateinit var keys:LoRaPairingKeys
    private var usb:LoRaUsbPort?=null
    private var ble:LoRaBlePort?=null
    private var activeRequest=0
    private var connectInProgress=false
    private var uploadCallback:ValueCallback<Array<Uri>>?=null
    private var microphonePermission:PermissionRequest?=null
    private var pendingNotificationPermission:Int?=null
    private var pendingPhoto:Pair<Int,ByteArray>?=null
    private var pendingBluetooth: (() -> Unit)?=null
    private val seenBle=linkedMapOf<String,BluetoothDevice>()
    private var scanning=false
    private val bluetoothPermissionLauncher=registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {
        val ok=requiredBlePermissions().all{ContextCompat.checkSelfPermission(this,it)==PackageManager.PERMISSION_GRANTED}
        if(ok)pendingBluetooth?.invoke() else failOpen("Bluetooth permission denied")
        pendingBluetooth=null
    }
    private val audioPermissionLauncher=registerForActivityResult(ActivityResultContracts.RequestPermission()){allowed ->
        val request=microphonePermission;microphonePermission=null
        if(allowed) request?.grant(arrayOf(PermissionRequest.RESOURCE_AUDIO_CAPTURE)) else request?.deny()
    }
    private val notificationPermissionLauncher=registerForActivityResult(ActivityResultContracts.RequestPermission()){allowed->
        val id=pendingNotificationPermission;pendingNotificationPermission=null
        if(id!=null)result(id,allowed,if(allowed)"" else "Notifications denied")
    }
    private val photoSave=registerForActivityResult(ActivityResultContracts.CreateDocument("image/*")){ uri ->
        val pending=pendingPhoto;pendingPhoto=null
        if(pending!=null) {
            try {
                if(uri==null)throw IllegalStateException("Save cancelled")
                contentResolver.openOutputStream(uri,"w")!!.use{it.write(pending.second)}
                result(pending.first,true,"")
            }catch(e:Exception){result(pending.first,false,e.message?:"Photo export error")}
        }
    }
    private val filePicker=registerForActivityResult(ActivityResultContracts.StartActivityForResult()){res ->
        val result=WebChromeClient.FileChooserParams.parseResult(res.resultCode,res.data)
        uploadCallback?.onReceiveValue(result);uploadCallback=null
    }
    private val scanCallback=object:ScanCallback(){
        override fun onScanResult(callbackType:Int,result:ScanResult) {
            val device=result.device
            val name=runCatching{device.name ?: result.scanRecord?.deviceName}.getOrNull().orEmpty()
            if(name.startsWith("OffGridLoRa-"))seenBle[device.address]=device
        }
        override fun onScanFailed(errorCode:Int) {main.post{failOpen("BLE scan error $errorCode")}}
    }
    override fun onCreate(savedInstanceState:Bundle?){
        super.onCreate(savedInstanceState)
        keys=LoRaPairingKeys(this)
        val loader=WebViewAssetLoader.Builder().addPathHandler("/assets/",WebViewAssetLoader.AssetsPathHandler(this)).build()
        web=WebView(this)
        setContentView(web,FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,ViewGroup.LayoutParams.MATCH_PARENT))
        web.settings.javaScriptEnabled=true
        web.settings.domStorageEnabled=true
        web.settings.allowFileAccess=false
        // The system photo picker returns content:// URIs with user-granted access.
        web.settings.allowContentAccess=true
        web.settings.allowFileAccessFromFileURLs=false
        web.settings.allowUniversalAccessFromFileURLs=false
        web.settings.mixedContentMode=android.webkit.WebSettings.MIXED_CONTENT_NEVER_ALLOW
        web.settings.mediaPlaybackRequiresUserGesture=true
        web.settings.setSupportMultipleWindows(false)
        web.webViewClient=object:WebViewClient(){
            override fun shouldInterceptRequest(view:WebView,request:WebResourceRequest):WebResourceResponse? {
                val url=request.url
                return if(url.scheme=="https"&&url.host=="appassets.androidplatform.net"&&url.path?.startsWith("/assets/www/")==true) {
                    loader.shouldInterceptRequest(url) ?: WebResourceResponse("text/plain","UTF-8",403,"Not found",mapOf(),ByteArrayInputStream(ByteArray(0)))
                }else WebResourceResponse("text/plain","UTF-8",403,"Offline only",mapOf(),ByteArrayInputStream(ByteArray(0)))
            }
            override fun shouldOverrideUrlLoading(view:WebView,request:WebResourceRequest):Boolean {
                // Never let remote HTML run in a privileged radio/crypto origin.
                return request.url.toString()!=START_URL
            }
            override fun onRenderProcessGone(view:WebView,detail:android.webkit.RenderProcessGoneDetail):Boolean {
                closeTransport();Toast.makeText(this@LoRaActivity,"WebView renderer stopped. Restart the app.",Toast.LENGTH_LONG).show();return true
            }
        }
        web.webChromeClient=object:WebChromeClient(){
            override fun onPermissionRequest(request:PermissionRequest) {
                main.post {
                    if(request.origin.scheme!="https" || request.origin.host!="appassets.androidplatform.net" ||
                       request.origin.port!=-1 || !request.resources.contentEquals(arrayOf(PermissionRequest.RESOURCE_AUDIO_CAPTURE))){
                        request.deny();return@post
                    }
                    if(ContextCompat.checkSelfPermission(this@LoRaActivity,Manifest.permission.RECORD_AUDIO)==PackageManager.PERMISSION_GRANTED)
                        request.grant(arrayOf(PermissionRequest.RESOURCE_AUDIO_CAPTURE))
                    else {microphonePermission?.deny();microphonePermission=request;audioPermissionLauncher.launch(Manifest.permission.RECORD_AUDIO)}
                }
            }
            override fun onShowFileChooser(view:WebView,cb:ValueCallback<Array<Uri>>,params:FileChooserParams):Boolean {
                uploadCallback?.onReceiveValue(null)
                uploadCallback=cb
                return try {filePicker.launch(params.createIntent());true}
                catch(e:Exception){uploadCallback=null;cb.onReceiveValue(null);false}
            }
        }
        if(!WebViewFeature.isFeatureSupported(WebViewFeature.WEB_MESSAGE_LISTENER)) {
            AlertDialog.Builder(this).setTitle("Android System WebView update needed")
                .setMessage("This app requires an up-to-date Android System WebView for its secure local-only radio bridge.")
                .setPositiveButton("OK",null).show()
            return
        }
        WebViewCompat.addWebMessageListener(web,"OffGridNative",setOf(ORIGIN)) {_,message,source,isMainFrame,_ ->
            if(!isMainFrame || source.scheme!="https" || source.host!="appassets.androidplatform.net" || source.port!=-1)return@addWebMessageListener
            main.post {runCatching{dispatch(JSONObject(message.data))}.onFailure{android.util.Log.w("OffGridAndroid","Bridge message rejected: ${it.javaClass.simpleName}")}}
        }
        web.loadUrl(START_URL)
    }
    private fun requiredBlePermissions():Array<String> = if(Build.VERSION.SDK_INT>=31)
        arrayOf(Manifest.permission.BLUETOOTH_CONNECT,Manifest.permission.BLUETOOTH_SCAN)
        else arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)

    private fun dispatch(msg:JSONObject) {
        val id=msg.optInt("id",0)
        if(id<1 || id>1000000000)return
        when(msg.optString("type")) {
            "open" -> {
                if(connectInProgress || usb!=null || ble!=null){result(id,false,"Already connected");return}
                activeRequest=id;connectInProgress=true;chooseTransport()
            }
            "write" -> {
                val bytes=runCatching{Base64.decode(msg.getString("data"),Base64.DEFAULT)}.getOrNull()
                if(bytes==null || bytes.size>16384){result(id,false,"Invalid transport frame");return}
                when {
                    usb!=null ->usb!!.write(bytes){ok,error->result(id,ok,error)}
                    ble!=null ->ble!!.write(bytes){ok,error->result(id,ok,error)}
                    else ->result(id,false,"ESP32 disconnected")
                }
            }
            "notifyPermission" ->requestNotificationPermission(id)
            "notify" -> {
                showNotification(msg.optString("title"),msg.optString("body"),msg.optString("tag"))
                result(id,true,"")
            }
            "clipboard" ->{
                val text=msg.optString("text").take(24000)
                (getSystemService(CLIPBOARD_SERVICE) as ClipboardManager).setPrimaryClip(ClipData.newPlainText("OffGrid",text))
                result(id,true,"")
            }
            "savePhoto" ->{
                val bytes=runCatching{Base64.decode(msg.getString("data"),Base64.DEFAULT)}.getOrNull()
                if(bytes==null||bytes.isEmpty()||bytes.size>100*1024){result(id,false,"Invalid photo export size");return}
                pendingPhoto=Pair(id,bytes)
                photoSave.launch(msg.optString("name","offgrid-photo.webp").replace(Regex("[^a-zA-Z0-9._-]"),"_").take(80))
            }
            "close" -> {closeTransport();result(id,true,"")}
        }
    }
    private fun requestNotificationPermission(id:Int){
        if(Build.VERSION.SDK_INT<33 || ContextCompat.checkSelfPermission(this,Manifest.permission.POST_NOTIFICATIONS)==PackageManager.PERMISSION_GRANTED) {
            result(id,true,"")
        }else {
            pendingNotificationPermission=id
            notificationPermissionLauncher.launch(Manifest.permission.POST_NOTIFICATIONS)
        }
    }
    private fun showNotification(title:String,body:String,tag:String){
        if(Build.VERSION.SDK_INT>=33 && ContextCompat.checkSelfPermission(this,Manifest.permission.POST_NOTIFICATIONS)!=PackageManager.PERMISSION_GRANTED)return
        val manager=getSystemService(NotificationManager::class.java)
        val channel="offgrid_lora_messages"
        manager.createNotificationChannel(NotificationChannel(channel,"Encrypted OffGrid messages",NotificationManager.IMPORTANCE_DEFAULT))
        val openIntent=PendingIntent.getActivity(this,2,Intent(this,LoRaActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
        val notice=NotificationCompat.Builder(this,channel)
            .setSmallIcon(R.mipmap.ic_launcher).setContentTitle(title.take(100)).setContentText(body.take(250))
            .setContentIntent(openIntent).setAutoCancel(true).build()
        manager.notify(tag.hashCode(),notice)
    }
    private fun chooseTransport(){
        val availableUsb=LoRaUsbPort.devices(this)
        val options=mutableListOf<String>()
        if(availableUsb.isNotEmpty())options.add("USB OTG · ESP32-C3 (works with existing firmware)")
        options.add("Bluetooth LE · OffGrid LoRa (requires BLE bulk firmware)")
        options.add("Original ESP-NOW Android app")
        AlertDialog.Builder(this).setTitle("Connect OffGrid radio")
            .setItems(options.toTypedArray()){_,which->
                when {
                    which<availableUsb.size && options[which].startsWith("USB") ->startUsb(availableUsb.first())
                    options[which].startsWith("Bluetooth") ->startBle()
                    else->{failOpen("Switched to classic app");startActivity(Intent(this,MainActivity::class.java))}
                }
            }.setOnCancelListener{failOpen("Connection cancelled")}.show()
    }
    private fun startUsb(device:android.hardware.usb.UsbDevice){
        usb=LoRaUsbPort(this,{rx(it)},{lost(it)})
        usb!!.open(device){ok,error->if(ok) {connectInProgress=false;result(activeRequest,true,"")}
            else{closeTransport();failOpen(error)} }
    }
    private fun startBle(){
        val permissions=requiredBlePermissions()
        if(permissions.any{ContextCompat.checkSelfPermission(this,it)!=PackageManager.PERMISSION_GRANTED}){
            pendingBluetooth={scanBle()};bluetoothPermissionLauncher.launch(permissions)
        }else scanBle()
    }
    private fun scanBle(){
        val adapter=getSystemService(BluetoothManager::class.java).adapter
        if(adapter==null || !adapter.isEnabled){failOpen("Turn on Bluetooth and retry");return}
        seenBle.clear();scanning=true
        val scanner=adapter.bluetoothLeScanner
        if(scanner==null){failOpen("BLE scanner not available");return}
        scanner.startScan(scanCallback)
        main.postDelayed({
            if(!scanning)return@postDelayed
            scanner.stopScan(scanCallback);scanning=false
            val nodes=seenBle.values.toList()
            if(nodes.isEmpty()){failOpen("No OffGridLoRa BLE nodes found; ensure the ESP32 is advertising");return@postDelayed}
            AlertDialog.Builder(this).setTitle("Select LoRa ESP32")
                .setItems(nodes.map{(runCatching{it.name}.getOrNull()?:"OffGridLoRa")+" · "+it.address}.toTypedArray()){_,which->selectBle(nodes[which])}
                .setOnCancelListener{failOpen("Bluetooth selection cancelled")}.show()
        },6000)
    }
    private fun selectBle(device:BluetoothDevice){
        val address=device.address
        val previous=keys.load(address)
        if(previous!=null){previous.fill(0);connectBle(address);return}
        val edit=EditText(this).apply{
            hint="64-character hex or 43-character base64url"
            inputType=InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_PASSWORD
            setSingleLine(true)
        }
        AlertDialog.Builder(this).setTitle("BLE pairing key")
            .setMessage("Enter the existing 256-bit key from the paired LoRa nodes. The key is protected by Android Keystore and is never read from the ESP32 over Bluetooth. Provision the nodes over physical USB first.")
            .setView(edit).setPositiveButton("Connect"){_,_->
                var key:ByteArray?=null
                try{key=LoRaPairingKeys.parse(edit.text.toString());keys.save(address,key);connectBle(address)}
                catch(e:Exception){failOpen("Invalid pairing key")}
                finally{key?.fill(0)}
            }.setNegativeButton("Cancel"){_,_->failOpen("Pairing cancelled")}.show()
    }
    private fun connectBle(mac:String){
        ble=LoRaBlePort(this,mac,keys,{rx(it)},{lost(it)})
        ble!!.open{ok,error->
            if(ok){connectInProgress=false;result(activeRequest,true,"")}
            else{if(error.contains("Authentication failed") || error.contains("Pairing key"))keys.forget(mac);closeTransport();failOpen(error)}
        }
        main.postDelayed({if(connectInProgress){closeTransport();failOpen("BLE authentication timed out")}},30000)
    }
    private fun failOpen(message:String){connectInProgress=false;result(activeRequest,false,message)}
    private fun closeTransport(){
        scanning=false
        runCatching{getSystemService(BluetoothManager::class.java).adapter?.bluetoothLeScanner?.stopScan(scanCallback)}
        usb?.close();usb=null
        ble?.close();ble=null
        connectInProgress=false
    }
    private fun lost(message:String){
        main.post {
            closeTransport()
            event(JSONObject().put("type","disconnect").put("reason",message))
        }
    }
    private fun rx(data:ByteArray){
        val base64=Base64.encodeToString(data,Base64.NO_WRAP)
        event(JSONObject().put("type","rx").put("data",base64))
    }
    private fun result(id:Int,success:Boolean,error:String){
        event(JSONObject().put("type","result").put("id",id).put("ok",success).put("error",error))
    }
    private fun event(obj:JSONObject){
        if(!::web.isInitialized)return
        val json=obj.toString()
        main.post {web.evaluateJavascript("window.__offgridAndroidSerial&&window.__offgridAndroidSerial.receive(JSON.parse(${JSONObject.quote(json)}))",null)}
    }
    override fun onBackPressed(){
        AlertDialog.Builder(this).setTitle("OffGrid Messenger")
            .setItems(arrayOf("Return to LoRa chat","Open original ESP-NOW app","Exit")){_,which->
                when(which){1->startActivity(Intent(this,MainActivity::class.java));2->finish()}
            }.show()
    }
    override fun onDestroy(){
        closeTransport()
        uploadCallback?.onReceiveValue(null);uploadCallback=null
        microphonePermission?.deny();microphonePermission=null
        if(::web.isInitialized)web.destroy()
        super.onDestroy()
    }
}
