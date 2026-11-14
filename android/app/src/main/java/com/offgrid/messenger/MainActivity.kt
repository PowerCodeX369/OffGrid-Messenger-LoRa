package com.offgrid.messenger

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.companion.AssociationInfo
import android.companion.AssociationRequest
import android.companion.BluetoothLeDeviceFilter
import android.companion.CompanionDeviceManager
import android.content.Intent
import android.net.Uri
import android.provider.Settings
import android.media.MediaPlayer
import android.graphics.BitmapFactory
import java.io.File
import java.util.concurrent.Executors
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.content.pm.PackageManager
import android.util.Log
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.enableEdgeToEdge
import androidx.core.content.ContextCompat
import androidx.core.view.WindowCompat
import androidx.activity.compose.setContent
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.IntentSenderRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.Send
import androidx.compose.material.icons.filled.Bluetooth
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material.icons.filled.Mic
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material.icons.filled.Image
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Stop
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.delay
import java.util.ArrayDeque
import java.util.regex.Pattern

private val Sapphire = Color(0xFF2563EB)
private val Navy = Color(0xFF0C172B)
private val PaleBlue = Color(0xFFEAF1FF)
private val Aqua = Color(0xFF60B9EE)
private val mainHandler = Handler(Looper.getMainLooper())

data class ChatItem(val id:String,val text:String,val outgoing:Boolean,val timestamp:Long,
    val status:String="sent",val kind:Int=OgmCodec.TEXT,val bytes:ByteArray?=null,
    val mime:String="",val width:Int=0,val height:Int=0,val durationMs:Int=0,val progress:Int=0, val speedKiBs:Float=0f, val edited:Boolean=false, val deleted:Boolean=false)
private data class Transfer(val frames:MutableList<LoRaNativeProtocol.Sealed>,val messageId:String,
    val control:Boolean=false,val media:Boolean=false,var index:Int=0,var attempts:Int=0)


class MainActivity : ComponentActivity() {
    private lateinit var secure: SecureKeyStore
    private var link: BleSessionHolder? = null
    private lateinit var history: EncryptedHistory
    private lateinit var notifications: OffGridNotifications
    private val historyWorker = Executors.newSingleThreadExecutor()
    private var scheduledHistory: Runnable? = null
    private var appResumed by mutableStateOf(false)
    private var chatScreenVisible by mutableStateOf(true)
    private var notificationsEnabled by mutableStateOf(true)
    private var notificationPreview by mutableStateOf(false)
    private var keepBleBackground by mutableStateOf(false)
    private var accessibleVoiceTap by mutableStateOf(false)
    private val mediaReceiver = LoRaNativeProtocol.MediaReceiver()
    private val completedBulkIds = linkedMapOf<String,String>()
    private val items = mutableStateListOf<ChatItem>()
    private val pending = ArrayDeque<Transfer>()
    private var isSending = false
    private var loRaCapable = false
    private var bleAuthenticated = false
    private var mediaMode = "idle" // idle, entering, active, restoring
    private var mediaTimer:Runnable? = null
    // POWER18 VERIFIED is a local, per-BLE-connection authorization: never persist it.
    // The app waits for firmware POWER18|ENABLED before requesting any high-PA profile.
    private var power18Confirmed = false
    private var powerApprovalRequest by mutableStateOf<String?>(null) // MEDIA or mode
    private var waitingForPowerAck: String? = null
    private var powerAckTimer: Runnable? = null
    private var mediaHintSent = false
    private var manualAfterAuto: String? = null
    private var manualAutoTimer: Runnable? = null
    private var requestedRadioMode: String? = null
    private var requestedRadioTimer: Runnable? = null
    private var fastPhotos by mutableStateOf(true)
    private val bulk by lazy { LoRaNativeBulk(
        write={command -> link?.send(command)},
        schedule={r,ms -> mainHandler.postDelayed(r,ms)},
        cancel={r -> mainHandler.removeCallbacks(r)},
        completed={ok,reason -> completedBulk(ok,reason)},
        transmitted={
            pending.firstOrNull()?.takeIf{!it.control && !it.media}?.let{updateReceipt(it.messageId,"sent")}
        },
        progress={n,total ->
            val job=pending.firstOrNull()
            if(job!=null && !job.control && total>0){
                val index=items.indexOfFirst{it.id==job.messageId}
                if(index>=0)items[index]=items[index].copy(progress=
                    ((job.index*100 + (100*n/total))/job.frames.size).coerceIn(0,100))
            }
        },
        received={peer,id,data -> receiveBulk(peer,id,data)}
    )}
    private var connected by mutableStateOf(false)
    private var radioLink by mutableStateOf<RadioLink?>(null)
    private var state by mutableStateOf("Not connected")
    private var trustedAddress by mutableStateOf<String?>(null)
    private var pairingSaved by mutableStateOf(false)
    private var details by mutableStateOf(false)
    private var dark by mutableStateOf(false)
    private var contactName by mutableStateOf("Nearby node")
    private var avatarBytes by mutableStateOf<ByteArray?>(null)
    private val bleDiagnostics = mutableStateListOf<String>()
    private var pickerHandled = false
    private val mediaWorker = Executors.newSingleThreadExecutor()
    private lateinit var voiceRecorder:VoiceNoteRecorder
    private var recording by mutableStateOf(false)
    private var playingId by mutableStateOf<String?>(null)
    private var voicePlayer:MediaPlayer?=null
    private var voicePlaybackFile:File?=null
    private var recordAutoStop:Runnable?=null
    private val playedReceipts=mutableSetOf<String>()
    private val hiddenMessageIds=mutableSetOf<String>()
    private var activityDestroyed=false

    private fun bleTrace(message: String) {
        val time = java.text.SimpleDateFormat("HH:mm:ss", java.util.Locale.US).format(java.util.Date())
        bleDiagnostics.add("$time  $message")
        while (bleDiagnostics.size > 24) bleDiagnostics.removeAt(0)
    }

    private fun acceptBleSelection(address: String?, origin: String) {
        val mac = address?.trim()?.uppercase(java.util.Locale.US)
        if (mac.isNullOrBlank() || !android.bluetooth.BluetoothAdapter.checkBluetoothAddress(mac)) {
            bleTrace("$origin returned no valid Bluetooth MAC")
            state = "Could not read selected BLE device; choose it again"
            return
        }
        if (pickerHandled && trustedAddress == mac) return
        pickerHandled = true
        trustedAddress = mac
        secure.saveNodeAddress(mac)
        bleTrace("BLE device selected from $origin: $mac")
        reconnectIfPermitted()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        secure = SecureKeyStore(this)
        history = EncryptedHistory(this)
        notifications = OffGridNotifications(this)
        val preferences=getSharedPreferences("offgrid_premium_24",MODE_PRIVATE)
        contactName=preferences.getString("contact_name","Nearby node") ?: "Nearby node"
        notifications.setContact(contactName,null)
        historyWorker.execute {
            runCatching {history.loadAvatar()}.onSuccess { data ->
                mainHandler.post {if(!activityDestroyed){avatarBytes=data;notifications.setContact(contactName,data)}}
            }
        }
        notificationsEnabled=preferences.getBoolean("notifications",true)
        notificationPreview=preferences.getBoolean("previews",false)
        keepBleBackground=preferences.getBoolean("ble_background",false)
        accessibleVoiceTap=preferences.getBoolean("accessible_voice_tap",false)
        historyWorker.execute {
            val hidden=runCatching { history.loadHiddenIds() }.getOrElse {
                mainHandler.post {bleTrace("Hidden message ledger unavailable: ${it.javaClass.simpleName}")}
                emptySet()
            }
            runCatching { history.load() }.onSuccess { stored ->
                mainHandler.post {
                    if(!activityDestroyed) {
                        hiddenMessageIds.addAll(hidden)
                        val present=items.map { it.id }.toSet()
                        val old=stored.filterNot {it.id in present || it.id in hiddenMessageIds}
                            .map { if(it.outgoing && it.status=="sending")it.copy(status="failed") else it }
                        items.addAll(0,old)
                        playedReceipts.addAll(old.filter { !it.outgoing && it.status=="played" }.map{it.id})
                        if(old.isNotEmpty())bleTrace("Restored ${old.size} encrypted messages from device")
                    }
                }
            }.onFailure { mainHandler.post { if(!activityDestroyed)bleTrace("Encrypted history unavailable: ${it.javaClass.simpleName}") } }
        }
        fastPhotos = getSharedPreferences("offgrid_media", MODE_PRIVATE).getBoolean("fast_photos", true)
        voiceRecorder = VoiceNoteRecorder(this)
        trustedAddress = secure.nodeAddress()
        pairingSaved = secure.readPairingKey()?.let { it.fill(0);true } ?: false
        dark = preferences.getBoolean("dark",(resources.configuration.uiMode and 0x30) == 0x20)
        link = BleSessionHolder
        BleSessionHolder.attach(this, {s,ready ->
            state=s;bleAuthenticated=ready
            if(ready){
                link?.send("CAPS?")
                connected=loRaCapable
                if(connected)startNext()
            }else{
                connected=false;loRaCapable=false
                radioLink=null;bulk.reset();mediaReceiver.clear()
                pending.firstOrNull()?.let{it.attempts=0}
                isSending=false;stopMediaTimer();mediaMode="idle";mediaHintSent=false
                power18Confirmed=false;powerApprovalRequest=null;waitingForPowerAck=null
                powerAckTimer?.let(mainHandler::removeCallbacks);powerAckTimer=null
                manualAfterAuto=null;manualAutoTimer?.let(mainHandler::removeCallbacks);manualAutoTimer=null
                requestedRadioMode=null;requestedRadioTimer?.let(mainHandler::removeCallbacks);requestedRadioTimer=null
                if(recording) {
                    recordAutoStop?.let(mainHandler::removeCallbacks);recordAutoStop=null
                    runCatching { voiceRecorder.finish(cancel=true) };recording=false
                }
            }
        }, ::consumeLine, ::bleTrace)
        setContent { OffGridScreen() }
        if(keepBleBackground) mainHandler.post { toggleBackground(true) }
        if(trustedAddress!=null && pairingSaved) mainHandler.post { reconnectIfPermitted() }
    }
    override fun onDestroy() {
        activityDestroyed=true
        stopVoicePlayback()
        if (recording) runCatching { voiceRecorder.finish(cancel=true) }
        recordAutoStop?.let(mainHandler::removeCallbacks)
        mediaWorker.shutdownNow()
        bulk.reset();mediaReceiver.clear();stopMediaTimer()
        powerAckTimer?.let(mainHandler::removeCallbacks)
        manualAutoTimer?.let(mainHandler::removeCallbacks)
        requestedRadioTimer?.let(mainHandler::removeCallbacks)
        scheduledHistory?.let(mainHandler::removeCallbacks)
        val finalSnapshot=items.toList()
        historyWorker.execute { runCatching { history.save(finalSnapshot) } }
        historyWorker.shutdown()
        BleSessionHolder.detach()
        if(!keepBleBackground && !isChangingConfigurations) link?.stop()
        super.onDestroy()
    }
    override fun onResume() { super.onResume(); appResumed=true }
    override fun onPause() { appResumed=false; super.onPause() }

    private fun scheduleHistorySave() {
        scheduledHistory?.let(mainHandler::removeCallbacks)
        val save=Runnable {
            if(!activityDestroyed) {
                val snapshot=items.toList()
                historyWorker.execute { runCatching { history.save(snapshot) }.onFailure {
                    Log.e("OffGridHistory","Encrypted history write failed",it)
                } }
            }
        }
        scheduledHistory=save
        mainHandler.postDelayed(save,250L)
    }
    private fun markVisible(ids:Set<String>) {
        if(!appResumed || !connected || ids.isEmpty())return
        var changed=false
        ids.forEach { id ->
            val index=items.indexOfFirst {it.id==id && !it.outgoing && it.status=="delivered"}
            if(index>=0){
                controlReceipt(OgmCodec.READ,id)
                items[index]=items[index].copy(status="read")
                changed=true
            }
        }
        if(changed)scheduleHistorySave()
    }
    private fun toggleBackground(enable:Boolean) {
        if(enable && !hasBluetoothPermission()){
            state="Grant Nearby devices permission before enabling background connection";return
        }
        val prefs=getSharedPreferences("offgrid_premium_24",MODE_PRIVATE)
        try {
            // Save before starting: onStartCommand can run before this method returns.
            prefs.edit().putBoolean("ble_background",enable).apply()
            if(enable) ContextCompat.startForegroundService(this,Intent(this,OffGridConnectionService::class.java))
            else stopService(Intent(this,OffGridConnectionService::class.java))
            keepBleBackground=enable
        } catch(ex:Exception){
            prefs.edit().putBoolean("ble_background",false).apply()
            state="Background service unavailable: ${ex.message}"
            keepBleBackground=false
        }
    }
    private fun retryFailed(item:ChatItem) {
        if(!connected || !item.outgoing || item.status!="failed")return
        val index=items.indexOfFirst {it.id==item.id}
        if(index<0)return
        items[index]=item.copy(status="sending",progress=0)
        when(item.kind){
            OgmCodec.TEXT -> pushEnvelope(item.kind,item.id,item.text.toByteArray(Charsets.UTF_8))
            OgmCodec.IMAGE -> item.bytes?.let {
                pushEnvelope(item.kind,item.id,MediaPayload.photoBody(item.mime,it,item.width,item.height),
                    width=item.width,height=item.height)
            }
            OgmCodec.VOICE -> item.bytes?.let {
                val dims=MediaPayload.durationParts(item.durationMs)
                pushEnvelope(item.kind,item.id,MediaPayload.voiceBody(item.mime,it,item.durationMs),
                    width=dims.first,height=dims.second)
            }
        }
        scheduleHistorySave()
    }
    private fun deleteForMe(item:ChatItem) {
        val idx=items.indexOfFirst { it.id==item.id }
        if(idx>=0) {
            if(playingId==item.id)stopVoicePlayback()
            // Suppress duplicate radio delivery and stale history restores first.
            hiddenMessageIds.add(item.id)
            items.removeAt(idx)
            val deletedSnapshot=hiddenMessageIds.toSet()
            historyWorker.execute {runCatching {history.saveHiddenIds(deletedSnapshot)} }
            scheduleHistorySave()
            state="Message removed from this device"
        }
    }
    private fun editOutgoing(item:ChatItem,updated:String) {
        if(!connected || !item.outgoing || item.deleted || item.kind!=OgmCodec.TEXT ||
            item.status !in listOf("delivered","read","played") ||
            updated.isBlank() || updated.codePointCount(0,updated.length)>MessageMutation.MAX_EDIT_CHARS)return
        val idx=items.indexOfFirst {it.id==item.id}
        if(idx<0)return
        val body=MessageMutation.editPayload(item.id,updated)
        pushEnvelope(OgmCodec.EDIT,OgmCodec.newId(),body,control=true)
        items[idx]=items[idx].copy(text=updated,edited=true)
        scheduleHistorySave()
    }
    private fun deleteForEveryone(item:ChatItem) {
        if(!connected || !item.outgoing || item.deleted ||
            item.status !in listOf("delivered","read","played"))return
        val idx=items.indexOfFirst {it.id==item.id}
        if(idx<0)return
        pushEnvelope(OgmCodec.DELETE,OgmCodec.newId(),MessageMutation.deletePayload(item.id),control=true)
        items[idx]=items[idx].copy(text="You deleted this message",bytes=null,deleted=true,edited=false)
        scheduleHistorySave()
    }

    private fun cancelVoiceRecording() {
        if(!recording)return
        recordAutoStop?.let(mainHandler::removeCallbacks);recordAutoStop=null
        runCatching { voiceRecorder.finish(cancel=true) }
        recording=false
        state="Voice recording cancelled"
    }
    private fun hasBluetoothPermission():Boolean = Build.VERSION.SDK_INT < 31 ||
        checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT)==PackageManager.PERMISSION_GRANTED
    private fun reconnectIfPermitted() {
        if(!hasBluetoothPermission()){state="Bluetooth permission required";return}
        if(!pairingSaved){state="Enter pairing key";return}
        trustedAddress?.let { link?.connect(it) }
    }

    private fun stopMediaTimer(){mediaTimer?.let(mainHandler::removeCallbacks);mediaTimer=null}
    private fun failPendingMedia(reason:String){
        stopMediaTimer();mediaMode="idle"
        val job=pending.firstOrNull()
        if(job!=null && job.media){pending.removeFirst();updateReceipt(job.messageId,"failed")}
        state=reason
        startNext()
    }
    private fun requestHighPower(action:String){
        if(power18Confirmed){
            if(action=="MEDIA")beginMediaSession() else applyRadioMode(action)
        }else if(powerApprovalRequest==null && waitingForPowerAck==null){
            powerApprovalRequest=action
        }
    }
    private fun declineHighPower(action:String){
        powerApprovalRequest=null
        if(action=="MEDIA"){
            // A declined confirmation is a conscious choice to use the
            // working current mode, not a silent claim of Fast handover.
            mediaHintSent=false;mediaMode="active"
            state="Media will use the current radio mode"
            startNext()
        }
    }
    private fun authorizeHighPower(action:String){
        if(!connected)return
        powerApprovalRequest=null
        waitingForPowerAck=action
        link?.send("POWER18 VERIFIED")
        powerAckTimer?.let(mainHandler::removeCallbacks)
        val timeout=Runnable {
            if(waitingForPowerAck==action){
                waitingForPowerAck=null
                if(action=="MEDIA")failPendingMedia("Fast power authorization timed out; retry media")
                else state="Power authorization timed out; no RF change applied"
            }
        }
        powerAckTimer=timeout;mainHandler.postDelayed(timeout,8_000L)
    }
    private fun emitRadioCommand(mode:String){
        val command=RadioLink.command(mode)?:return
        requestedRadioTimer?.let(mainHandler::removeCallbacks)
        requestedRadioMode=mode
        link?.send(command)
        state="Requesting synchronized LoRa ${mode.lowercase()} mode…"
        val timeout=Runnable {
            if(requestedRadioMode==mode){
                requestedRadioMode=null
                state="No authenticated RF mode confirmation; check both nodes and their LoRa signal"
                Toast.makeText(this, state, Toast.LENGTH_LONG).show()
                link?.send("STATUS")
            }
        }
        requestedRadioTimer=timeout
        // A signed SF12 exchange can require multiple seconds of airtime per
        // OFFER/AGREE/HELLO. Do not tell the user it failed after 24 seconds.
        mainHandler.postDelayed(timeout,if(mode=="ULTRA" || radioLink?.activeProfile=="ULTRA") 90_000L else 24_000L)
    }
    private fun clearRadioRequest(){
        requestedRadioMode=null
        requestedRadioTimer?.let(mainHandler::removeCallbacks)
        requestedRadioTimer=null
    }
    private fun applyRadioMode(mode:String){
        if(!connected)return
        if(RadioLink.command(mode)==null)return
        if(mode!="AUTO" && radioLink?.rateMode=="AUTO"){
            // AUTO OFF is an authenticated two-node handshake; never send the
            // manual profile offer until that handshake has completed.
            manualAfterAuto=mode
            link?.send("AUTO OFF")
            state="Disabling Auto on both radios before manual selection…"
            manualAutoTimer?.let(mainHandler::removeCallbacks)
            val timeout=Runnable {
                if(manualAfterAuto==mode){manualAfterAuto=null;state="Auto OFF not confirmed; mode unchanged"}
            }
            manualAutoTimer=timeout;mainHandler.postDelayed(timeout,
                if(radioLink?.activeProfile=="ULTRA") 60_000L else 12_000L)
        }else emitRadioCommand(mode)
        mainHandler.postDelayed({if(connected)link?.send("STATUS")},2_000L)
    }
    private fun chooseRadioMode(mode:String){
        if(!connected || mode !in RadioLink.choices)return
        if(isSending || bulk.busy || pending.isNotEmpty() || mediaMode!="idle"){
            state="Finish outgoing messages and media before changing RF modes"
            return
        }
        if(RadioLink.requiresHighPower(mode))requestHighPower(mode)
        else applyRadioMode(mode)
    }
    private fun beginMediaSession(){
        if(radioLink?.activeProfile=="ULTRA"){failPendingMedia("Ultra SF12 is text only. Leave Ultra before media");return}
        if(!power18Confirmed){requestHighPower("MEDIA");return}
        mediaMode="entering";mediaHintSent=true
        link?.send("MEDIA ON")
        stopMediaTimer()
        val timeout=Runnable {
            if(mediaMode=="entering"){
                link?.send("MEDIA OFF")
                failPendingMedia("Media RF synchronization timed out; retry after both nodes agree")
            }
        }
        mediaTimer=timeout;mainHandler.postDelayed(timeout,22_000L)
    }
    private fun endMediaSession(){
        if(mediaMode=="idle"){startNext();return}
        if(!mediaHintSent){
            mediaMode="idle";startNext();return
        }
        mediaMode="restoring"
        link?.send("MEDIA OFF")
        stopMediaTimer()
        val timeout=Runnable {
            if(mediaMode=="restoring"){
                mediaMode="idle";mediaHintSent=false
                state="RF restore not confirmed; check both nodes' active profiles"
                startNext()
            }
        }
        mediaTimer=timeout;mainHandler.postDelayed(timeout,18_000L)
    }
    private fun completedBulk(ok:Boolean,reason:String){
        val job=pending.firstOrNull()?:run{isSending=false;return}
        isSending=false
        if(!ok){
            if(job.attempts++<2 && connected){
                state="Retrying unconfirmed LoRa transfer"
                // A new AES-GCM nonce + transfer ID avoids the radio bridge's
                // completed-transfer replay cache, while the 64-bit message ID
                // stays identical for authenticated app-level deduplication.
                val key=secure.readPairingKey()
                if(key!=null){
                    try{
                        val old=job.frames[job.index]
                        val env=LoRaNativeProtocol.open(key,old.transferId,old.ciphertext)
                        job.frames[job.index]=LoRaNativeProtocol.seal(key,env.kind,env.id,env.text,
                            env.width,env.height,env.timestamp/1000)
                    }catch(e:Exception){bleTrace("Encrypted retry preparation failed")}
                    finally{key.fill(0)}
                }
                mainHandler.postDelayed({if(connected)startNext()},350L*job.attempts)
            }else{
                pending.removeFirst()
                if(!job.control)updateReceipt(job.messageId,"failed")
                state="LoRa confirmation unavailable; retry the message"
                if(job.media)endMediaSession() else startNext()
            }
            return
        }
        job.attempts=0;job.index++
        if(job.media){
            val index=items.indexOfFirst{it.id==job.messageId}
            if(index>=0)items[index]=items[index].copy(progress=100*job.index/job.frames.size)
        }
        if(job.index==job.frames.size){
            pending.removeFirst()
            if(!job.control)updateReceipt(job.messageId,"sent")
            if(job.media)endMediaSession() else startNext()
        }else{
            // A completed media ciphertext segment is the only safe BLE/ARQ
            // preemption boundary. Keep the same media Fast session active.
            val urgent=pending.firstOrNull{it!==job && (!it.media || it.control)}
            if(urgent!=null){pending.remove(urgent);pending.addFirst(urgent)}
            startNext()
        }
    }
    private fun startNext(){
        if(!connected||isSending||bulk.busy||mediaMode=="entering"||mediaMode=="restoring" ||
           powerApprovalRequest!=null || waitingForPowerAck!=null)return
        val front=pending.firstOrNull()?:return
        if(!front.control && !front.media){
            val receipt=pending.firstOrNull{it.control}
            if(receipt!=null){pending.remove(receipt);pending.addFirst(receipt)}
        }
        val job=pending.firstOrNull()?:return
        if(job.media && radioLink?.activeProfile=="ULTRA") {
            pending.removeFirst();updateReceipt(job.messageId,"failed")
            state="Ultra SF12 is text only; media was cancelled when the RF mode changed"
            if(mediaMode!="idle")endMediaSession() else startNext()
            return
        }
        if(job.media && mediaMode=="idle"){beginMediaSession();return}
        isSending=true
        try{bulk.start(job.frames[job.index],job.control,job.media,radioLink?.activeProfile=="ULTRA")}catch(e:Exception){
            isSending=false;state="LoRa staging unavailable: ${e.message}"
            pending.removeFirst()
            if(!job.control)updateReceipt(job.messageId,"failed")
            if(job.media)endMediaSession() else startNext()
        }
    }
    private fun pushEnvelope(kind:Int,messageId:String,body:ByteArray,control:Boolean=false,width:Int=0,height:Int=0){
        val key=secure.readPairingKey() ?: run{state="Pairing key required";return}
        try{
            val media=(kind==OgmCodec.IMAGE || kind==OgmCodec.VOICE)
            if(media && radioLink?.activeProfile=="ULTRA")throw IllegalStateException("Ultra is encrypted text only")
            val parts:List<LoRaNativeProtocol.Sealed> = if(media){
                val mime=if(kind==OgmCodec.IMAGE)listOf("image/webp","image/png","image/jpeg")
                    .getOrNull(body[0].toInt() and 255) else listOf("audio/webm","audio/ogg","audio/mp4")
                    .getOrNull(body[0].toInt() and 255)
                require(mime!=null && body.size>=257)
                val tsSeconds=System.currentTimeMillis()/1000
                LoRaNativeProtocol.mediaParts(kind,mime,body.copyOfRange(1,body.size)).map{part->
                    LoRaNativeProtocol.seal(key,OgmCodec.MEDIA_PART,messageId,part,width,height,tsSeconds)
                }
            }else listOf(LoRaNativeProtocol.seal(key,kind,messageId,body,width,height))
            pending.addLast(Transfer(parts.toMutableList(),messageId,control,media))
            startNext()
        }catch(ex:Exception){state="Encryption/LoRa media error: ${ex.message}"}
        finally{key.fill(0)}
    }
    private fun sendText(text:String) {
        if(!connected)return
        val value=text.trim()
        if(value.isEmpty()||value.length>500)return
        val id=OgmCodec.newId()
        items.add(ChatItem(id,value,true,System.currentTimeMillis(),"sending"))
        scheduleHistorySave()
        pushEnvelope(OgmCodec.TEXT,id,value.toByteArray(Charsets.UTF_8))
    }
    private fun sendPhoto(uri:Uri) {
        if(radioLink?.activeProfile=="ULTRA"){state="Ultra SF12 is text only. Photos are disabled";return}
        // In-app CameraX captures only temporary JPEGs in our own cache.
        // Gallery/content URIs are never deleted.
        val capturedFile=if(uri.scheme=="file") uri.path?.let(::File)?.takeIf {
            it.parentFile?.canonicalPath==cacheDir.canonicalPath && it.name.startsWith("offgrid-camera-")
        } else null
        if(!connected) {
            capturedFile?.delete()
            state="Connect before sending a photo"
            return
        }
        state="Preparing private photo…"
        val targetBytes = if(fastPhotos) MediaPayload.FAST_PHOTO_TARGET else MediaPayload.ANDROID_PHOTO_TARGET
        mediaWorker.execute {
            val result=try {runCatching { PhotoPreparer.prepare(contentResolver,uri,targetBytes) }}
                finally {capturedFile?.delete()}
            mainHandler.post {
                if(activityDestroyed)return@post
                result.onSuccess { photo ->
                    if(!connected){state="Photo preparation finished, but node disconnected";return@onSuccess}
                    val id=OgmCodec.newId()
                    val body=MediaPayload.photoBody(photo.mime,photo.bytes,photo.width,photo.height)
                    items.add(ChatItem(id,"Photo",true,System.currentTimeMillis(),"sending",
                        OgmCodec.IMAGE,photo.bytes,photo.mime,photo.width,photo.height))
                    scheduleHistorySave()
                    pushEnvelope(OgmCodec.IMAGE,id,body,width=photo.width,height=photo.height)
                    state="Encrypted photo queued · ${photo.bytes.size/1024} KiB"
                }.onFailure { state="Photo not sent: ${it.message}" }
            }
        }
    }
    private fun startVoiceRecording() {
        if(radioLink?.activeProfile=="ULTRA"){state="Ultra SF12 is text only. Voice notes are disabled";return}
        if(!connected){state="Connect to record";return}
        try {
            voiceRecorder.start();recording=true
            state="Recording voice · release to send (120s/40 KiB max)"
            val timeout=Runnable { if(recording) finishVoiceRecording() }
            recordAutoStop=timeout
            mainHandler.postDelayed(timeout,120_000L)
            scheduleVoiceSizeMonitor()
        } catch(ex:Exception) { state="Microphone unavailable: ${ex.message}";recording=false }
    }
    private fun scheduleVoiceSizeMonitor(){
        mainHandler.postDelayed({
            if(recording){
                if(voiceRecorder.estimatedFileBytes()>=38*1024)finishVoiceRecording()
                else scheduleVoiceSizeMonitor()
            }
        },400L)
    }
    private fun finishVoiceRecording() {
        if(!recording)return
        recordAutoStop?.let(mainHandler::removeCallbacks);recordAutoStop=null
        recording=false
        val result=runCatching { voiceRecorder.finish() }
        result.onSuccess { voice ->
            if(voice==null||!connected) { state="Recording discarded";return@onSuccess }
            try {
                val id=OgmCodec.newId()
                val body=MediaPayload.voiceBody(voice.mime,voice.bytes,voice.durationMs)
                val (w,h)=MediaPayload.durationParts(voice.durationMs)
                items.add(ChatItem(id,"Voice note",true,System.currentTimeMillis(),"sending",
                    OgmCodec.VOICE,voice.bytes,voice.mime,durationMs=voice.durationMs))
                scheduleHistorySave()
                pushEnvelope(OgmCodec.VOICE,id,body,width=w,height=h)
                state="Encrypted voice note queued · ${voice.bytes.size/1024} KiB"
            } catch(ex:Exception) { state="Voice not sent: ${ex.message}" }
        }.onFailure { state="Voice recording failed: ${it.message}" }
    }
    private fun stopVoicePlayback() {
        runCatching { voicePlayer?.release() }
        voicePlayer=null
        voicePlaybackFile?.delete();voicePlaybackFile=null
        playingId=null
    }
    private fun toggleVoicePlayback(item:ChatItem) {
        if(playingId==item.id){stopVoicePlayback();return}
        stopVoicePlayback()
        val raw=item.bytes ?: return
        try {
            val suffix=when(item.mime){"audio/mp4"->".m4a";"audio/ogg"->".ogg";else->".webm"}
            val file=File.createTempFile("offgrid-play-",suffix,cacheDir)
            voicePlaybackFile=file
            file.writeBytes(raw)
            val player=MediaPlayer()
            voicePlayer=player
            player.setDataSource(file.absolutePath)
            player.setOnCompletionListener {
                stopVoicePlayback()
                // 'Played' is only sent after the listener finishes the audio.
                if(!item.outgoing && playedReceipts.add(item.id)) {
                    controlReceipt(OgmCodec.PLAYED,item.id)
                    items.indexOfFirst{it.id==item.id}.takeIf{it>=0}?.let{idx ->items[idx]=items[idx].copy(status="played")}
                    scheduleHistorySave()
                }
            }
            player.setOnErrorListener { _,_,_ -> stopVoicePlayback();state="Audio decoder error";true }
            player.prepare();player.start();playingId=item.id
        } catch(ex:Exception){ stopVoicePlayback();state="Cannot play ${item.mime}: ${ex.message}" }
    }

    private fun controlReceipt(kind:Int,targetId:String) {
        val id=OgmCodec.newId()
        pushEnvelope(kind,id,OgmCodec.unhex(targetId),control=true)
    }
    private fun updateReceipt(targetId:String,status:String) {
        val index=items.indexOfFirst { it.id==targetId && it.outgoing }
        if(index>=0) {
            val current=items[index]
            val next=ReceiptStates.advance(current.status,status)
            if(current.status!=next){items[index]=current.copy(status=next);scheduleHistorySave()}
        }
    }
    private fun receiveBulk(peer:String,transferId:String,ciphertext:ByteArray){
        val unique="$peer|$transferId"
        completedBulkIds[unique]?.let{id->
            val existing=items.firstOrNull{it.id==id && !it.outgoing}
            if(existing!=null){
                controlReceipt(OgmCodec.DELIVERED,id)
                if(existing.status in listOf("read","played"))controlReceipt(OgmCodec.READ,id)
            }
            return
        }
        val key=secure.readPairingKey()?:return
        try{
            // AES-GCM authentication is mandatory before any message, receipt,
            // or media part can change the native UI or encrypted history.
            val env=LoRaNativeProtocol.open(key,transferId,ciphertext)
            if(env.kind==OgmCodec.MEDIA_PART){
                val part=mediaReceiver.accept(peer,env.id,env.text,env.width,env.height,env.timestamp)
                if(part!=null){
                    if(env.id in hiddenMessageIds){controlReceipt(OgmCodec.DELIVERED,env.id)}
                    else if(items.none{it.id==env.id}){
                        val chat=if(part.kind==OgmCodec.IMAGE)
                            ChatItem(env.id,"Photo",false,env.timestamp,"delivered",OgmCodec.IMAGE,
                                part.bytes,part.mime,part.width,part.height)
                        else ChatItem(env.id,"Voice note",false,env.timestamp,"delivered",OgmCodec.VOICE,
                                part.bytes,part.mime,durationMs=MediaPayload.durationFromParts(part.width,part.height))
                        items.add(chat);scheduleHistorySave();controlReceipt(OgmCodec.DELIVERED,env.id)
                        if(notificationsEnabled && (!appResumed||!chatScreenVisible))notifications.newMessage(chat,notificationPreview)
                    } else controlReceipt(OgmCodec.DELIVERED,env.id)
                }
            }else if(env.kind in listOf(OgmCodec.TEXT,OgmCodec.IMAGE,OgmCodec.VOICE)){
                if(env.id in hiddenMessageIds){controlReceipt(OgmCodec.DELIVERED,env.id)}
                else if(items.none{it.id==env.id}){
                    val chat=when(env.kind){
                        OgmCodec.TEXT->{
                            val text=env.text.toString(Charsets.UTF_8)
                            require(text.isNotBlank() && text.length<=500)
                            ChatItem(env.id,text,false,env.timestamp,"delivered")
                        }
                        OgmCodec.IMAGE->{
                            val picture=MediaPayload.unpackPhoto(env.text,env.width,env.height)
                            ChatItem(env.id,"Photo",false,env.timestamp,"delivered",OgmCodec.IMAGE,
                                picture.bytes,picture.mime,picture.width,picture.height)
                        }
                        else->{
                            val voice=MediaPayload.unpackVoice(env.text,MediaPayload.durationFromParts(env.width,env.height))
                            ChatItem(env.id,"Voice note",false,env.timestamp,"delivered",OgmCodec.VOICE,
                                voice.bytes,voice.mime,durationMs=voice.durationMs)
                        }
                    }
                    items.add(chat);scheduleHistorySave();controlReceipt(OgmCodec.DELIVERED,env.id)
                    if(notificationsEnabled && (!appResumed||!chatScreenVisible))notifications.newMessage(chat,notificationPreview)
                }else controlReceipt(OgmCodec.DELIVERED,env.id)
            }else when(env.kind){
                OgmCodec.DELIVERED,OgmCodec.READ,OgmCodec.PLAYED -> if(env.text.size==8){
                    updateReceipt(OgmCodec.hex(env.text),when(env.kind){
                        OgmCodec.DELIVERED->"delivered";OgmCodec.READ->"read";else->"played"})
                }
                OgmCodec.RECEIPT_BATCH -> LoRaNativeProtocol.readReceiptBatch(env.text).forEach{
                    updateReceipt(it.id,if(it.kind==OgmCodec.READ)"read" else "delivered")
                }
                OgmCodec.EDIT,OgmCodec.DELETE -> {
                    val target=MessageMutation.target(env.text)
                    val index=items.indexOfFirst{it.id==target&&!it.outgoing}
                    if(index>=0){
                        if(env.kind==OgmCodec.DELETE&&playingId==target)stopVoicePlayback()
                        MessageMutation.apply(items[index],env.kind,env.text)?.let{
                            items[index]=it;scheduleHistorySave()
                        }
                    }
                }
            }
            completedBulkIds[unique]=env.id
            while(completedBulkIds.size>512)completedBulkIds.remove(completedBulkIds.keys.first())
        }catch(e:Exception){Log.w("OffGridLoRa","Rejected invalid authenticated LoRa payload",e)}
        finally{key.fill(0)}
    }
    private fun consumeLine(line:String){
        if(bulk.onLine(line))return
        val split=line.split('|')
        when(split.firstOrNull()){
            "CAPS" -> {
                val fields=split.drop(1).toSet()
                val valid=fields.containsAll(setOf("FAMILY=LORA","PROTO=2","MEDIA=1","BULK=1"))
                loRaCapable=valid
                connected=bleAuthenticated && valid
                state=if(connected)"Connected · authenticated LoRa BLE" else
                    "LoRa BLE bulk unavailable: flash the companion BLE firmware on both radios"
                if(connected)startNext()
            }
            "READY" -> if(connected)state="ESP32 LoRa ready"
            "LINK" -> radioLink=RadioLink.parse(line)
            "PROFILE","AUTO","MEDIA","POWER18" -> {
                bleTrace(line.take(180))
                val event=split.getOrNull(1)
                when(split[0]){
                    "POWER18" -> if(event=="ENABLED"){
                        powerAckTimer?.let(mainHandler::removeCallbacks);powerAckTimer=null
                        power18Confirmed=true
                        val action=waitingForPowerAck;waitingForPowerAck=null
                        if(action=="MEDIA")beginMediaSession()
                        else if(action!=null)applyRadioMode(action)
                    }else if(event=="ERROR"){
                        powerAckTimer?.let(mainHandler::removeCallbacks);powerAckTimer=null
                        val action=waitingForPowerAck;waitingForPowerAck=null
                        power18Confirmed=false
                        if(action=="MEDIA")failPendingMedia("LoRa high-power authorization rejected: $line")
                        else state="LoRa high-power authorization rejected"
                    }
                    "AUTO" -> {
                        if(event=="OFF" && manualAfterAuto!=null){
                            val target=manualAfterAuto!!;manualAfterAuto=null
                            manualAutoTimer?.let(mainHandler::removeCallbacks);manualAutoTimer=null
                            emitRadioCommand(target)
                        }else if(event=="ERROR"){
                            manualAfterAuto=null
                            manualAutoTimer?.let(mainHandler::removeCallbacks);manualAutoTimer=null
                            clearRadioRequest()
                            state="Auto RF rejected by ESP32: $line"
                            Toast.makeText(this, state, Toast.LENGTH_LONG).show()
                        }else if(event=="ON" && requestedRadioMode=="AUTO"){
                            clearRadioRequest();state="Automatic LoRa adaptation synchronized on both radios"
                        }else if(event=="OFF" && requestedRadioMode=="AUTO"){
                            clearRadioRequest();state="Auto RF disabled; manually select Auto again"
                        }
                    }
                    "PROFILE" -> {
                        if(event=="ERROR" || event=="TIMEOUT" || event=="PEER_BUSY"){
                            clearRadioRequest()
                            state="LoRa profile change failed: $line"
                            Toast.makeText(this, state, Toast.LENGTH_LONG).show()
                        }else if(event=="ACTIVE"){
                            val actual=when(split.getOrNull(2)){
                                "LOW"->"FAST";"BALANCED"->"BALANCED";"MAX"->"RANGE";"ULTRA"->"ULTRA";else->null
                            }
                            if(actual!=null && actual==requestedRadioMode){
                                clearRadioRequest();state="Both radios synchronized: $actual"
                            }
                        }
                    }
                    "MEDIA" -> {
                        when {
                            mediaMode=="entering" && event=="READY" -> {
                                stopMediaTimer();mediaMode="active";startNext()
                            }
                            mediaMode=="entering" && event=="BYPASS" -> {
                                // Firmware explicitly declined the Fast handover;
                                // media can still use its verified existing profile.
                                stopMediaTimer();mediaMode="active"
                                state="Fast unavailable (${split.getOrNull(2) ?: "RF conditions"}); sending at current mode"
                                startNext()
                            }
                            mediaMode=="entering" && event=="ERROR" -> {
                                link?.send("MEDIA OFF")
                                failPendingMedia("Media synchronization rejected: $line")
                            }
                            mediaMode=="restoring" && event in listOf("RESTORED","OFF","RESTORE_FAILED") -> {
                                stopMediaTimer();mediaMode="idle";mediaHintSent=false
                                if(event=="RESTORE_FAILED")state="Restore failed; manually synchronize both RF modes"
                                startNext()
                            }
                        }
                    }
                }
            }
            "ERROR" -> {
                state="Node: ${split.drop(1).joinToString(" ")}";bleTrace(state)
            }
        }
    }
    @SuppressLint("MissingPermission")
    private fun startSystemPairing(launch: (IntentSenderRequest) -> Unit) {
        if (!hasBluetoothPermission()) { state="Grant Nearby devices permission"; return }
        pickerHandled = false
        try {
            val companion = getSystemService(CompanionDeviceManager::class.java)
            val filter = BluetoothLeDeviceFilter.Builder()
                .setNamePattern(Pattern.compile("^(OffGrid-.*|OffGridLoRa-.*)$"))
                .build()
            val request = AssociationRequest.Builder()
                .addDeviceFilter(filter)
                .setSingleDevice(false)
                .build()
            val callback = object : CompanionDeviceManager.Callback() {
                @Deprecated("For Android 8-12")
                override fun onDeviceFound(chooserLauncher: android.content.IntentSender) {
                    runOnUiThread { launch(IntentSenderRequest.Builder(chooserLauncher).build()) }
                }
                override fun onAssociationPending(chooserLauncher: android.content.IntentSender) {
                    runOnUiThread { launch(IntentSenderRequest.Builder(chooserLauncher).build()) }
                }
                override fun onAssociationCreated(associationInfo: AssociationInfo) {
                    // Android 13+ may deliver the chosen MAC here instead of in the Activity result.
                    runOnUiThread {
                        acceptBleSelection(associationInfo.deviceMacAddress?.toString(), "association callback")
                    }
                }
                override fun onFailure(error: CharSequence?) {
                    runOnUiThread {
                        bleTrace("Companion device association failed: $error")
                        state="BLE device selection failed: $error"
                    }
                }
            }
            // Modern executor overload reports AssociationInfo; old callback supports Android 8-12.
            if (Build.VERSION.SDK_INT >= 33) {
                companion.associate(request, mainExecutor, callback)
            } else {
                companion.associate(request, callback, Handler(Looper.getMainLooper()))
            }
            bleTrace("Companion device picker opened")
            state="Select your nearby OffGrid node"
        } catch (ex: Exception) {
            bleTrace("Device picker exception: ${ex.javaClass.simpleName}: ${ex.message}")
            state="Pairing unavailable: ${ex.message}"
        }
    }

    private fun addressFromPicker(data: Intent?): String? {
        if (data == null) return null
        // CompanionDeviceManager.EXTRA_DEVICE is NOT BluetoothDevice.EXTRA_DEVICE.
        // Selecting a BLE device uses the companion-specific key on older Android versions.
        val device = if (Build.VERSION.SDK_INT >= 33) {
            data.getParcelableExtra(CompanionDeviceManager.EXTRA_DEVICE, BluetoothDevice::class.java)
        } else {
            @Suppress("DEPRECATION")
            val old = data.getParcelableExtra<BluetoothDevice>(CompanionDeviceManager.EXTRA_DEVICE)
            old
        }
        if (device != null) return device.address
        if (Build.VERSION.SDK_INT >= 33) {
            data.getParcelableExtra(CompanionDeviceManager.EXTRA_ASSOCIATION, AssociationInfo::class.java)
                ?.deviceMacAddress?.toString()?.let { return it }
        }
        // Defensive fallback for vendor-specific companion chooser implementations.
        val legacy = if (Build.VERSION.SDK_INT >= 33) {
            data.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE, BluetoothDevice::class.java)
        } else {
            @Suppress("DEPRECATION")
            val old = data.getParcelableExtra<BluetoothDevice>(BluetoothDevice.EXTRA_DEVICE)
            old
        }
        return legacy?.address
    }

    @Composable
    private fun OffGridScreen() {
        var keyInput by remember { mutableStateOf("") }
        var showKey by remember { mutableStateOf(!pairingSaved) }
        val systemPicker=rememberLauncherForActivityResult(ActivityResultContracts.StartIntentSenderForResult()) { result ->
            if(result.resultCode==RESULT_OK){
                val address=addressFromPicker(result.data)
                if(address!=null)acceptBleSelection(address,"chooser result")
                else if(!pickerHandled)state="Could not read selected BLE address; select again"
            } else if(!pickerHandled)state="Device selection cancelled"
        }
        val bluetoothPermission=rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
            if(!granted)state="Nearby devices permission denied"
            else if(trustedAddress==null && pairingSaved)startSystemPairing(systemPicker::launch)
            else reconnectIfPermitted()
        }
        val notificationsPermission=rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
            if(!granted)state="Notification permission denied; enable in Android Settings"
        }
        var inAppCamera by remember { mutableStateOf(false) }
        val cameraPermission=rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
            if(granted) inAppCamera=true else state="Camera permission denied; allow in Android Settings"
        }
        val photoPicker=rememberLauncherForActivityResult(ActivityResultContracts.GetContent()) { uri:Uri? ->
            if(uri!=null)sendPhoto(uri)
        }
        val avatarPicker=rememberLauncherForActivityResult(ActivityResultContracts.GetContent()) { uri:Uri? ->
            if(uri!=null)mediaWorker.execute {
                runCatching {
                    val opts=BitmapFactory.Options().apply {inJustDecodeBounds=true}
                    contentResolver.openInputStream(uri)?.use {BitmapFactory.decodeStream(it,null,opts)}
                    val sample=BitmapFactory.Options().apply {
                        inSampleSize=(maxOf(opts.outWidth,opts.outHeight)/256).coerceAtLeast(1)
                    }
                    val bitmap=contentResolver.openInputStream(uri)?.use {BitmapFactory.decodeStream(it,null,sample)}
                        ?: error("Cannot decode contact image")
                    val target=android.graphics.Bitmap.createScaledBitmap(bitmap,160,160,true)
                    if(bitmap!==target)bitmap.recycle()
                    val output=java.io.ByteArrayOutputStream()
                    target.compress(android.graphics.Bitmap.CompressFormat.JPEG,78,output)
                    target.recycle()
                    output.toByteArray().also {require(it.size<=32768)}
                }.onSuccess {data ->
                    historyWorker.execute {runCatching {history.saveAvatar(data)}.onFailure {
                        Log.e("OffGridHistory","Could not encrypt contact avatar",it)
                    }}
                    mainHandler.post {if(!activityDestroyed){avatarBytes=data;notifications.setContact(contactName,data)}}
                }.onFailure {err -> mainHandler.post {state="Contact photo failed: ${err.message}"} }
            }
        }
        val micPermission=rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
            state=if(granted)"Microphone ready · hold mic button to record" else "Microphone permission required"
        }
        LaunchedEffect(inAppCamera) {
            if(inAppCamera) chatScreenVisible=false // camera must not mark off-screen chats read
        }
        val prefs=getSharedPreferences("offgrid_premium_24",MODE_PRIVATE)
        var notificationAsked by remember { mutableStateOf(false) }
        LaunchedEffect(connected,notificationsEnabled) {
            if(connected && notificationsEnabled && !notificationAsked && Build.VERSION.SDK_INT>=33 && !notifications.canNotify()) {
                notificationAsked=true
                notificationsPermission.launch(Manifest.permission.POST_NOTIFICATIONS)
            }
        }
        SideEffect {
            val insets=WindowCompat.getInsetsController(window,window.decorView)
            insets.isAppearanceLightStatusBars=!dark && !inAppCamera
            insets.isAppearanceLightNavigationBars=!dark && !inAppCamera
        }
        OffGridPremiumTheme(dark) {
            if(inAppCamera) {
                InAppCameraScreen(
                    onCancel={inAppCamera=false},
                    onSend={capturedFile ->
                        inAppCamera=false
                        sendPhoto(Uri.fromFile(capturedFile))
                    }
                )
            } else PremiumMessenger(
                items=items.toList(),connected=connected,status=state,nodeAddress=trustedAddress,
                radioLink=radioLink,
                dark=dark,fastPhotos=fastPhotos,notifications=notificationsEnabled,
                privatePreview=notificationPreview,backgroundEnabled=keepBleBackground,
                recording=recording,playingId=playingId,appVisible=appResumed,
                accessibleVoiceTap=accessibleVoiceTap,
                diagnostics=bleDiagnostics.toList(),contactName=contactName,avatarBytes=avatarBytes,
                actions=PremiumActions(
                    sendText=::sendText,photo={photoPicker.launch("image/*")},
                    camera={
                        if(checkSelfPermission(Manifest.permission.CAMERA)==PackageManager.PERMISSION_GRANTED)
                            inAppCamera=true
                        else cameraPermission.launch(Manifest.permission.CAMERA)
                    },
                    micPress={
                        if(checkSelfPermission(Manifest.permission.RECORD_AUDIO)==PackageManager.PERMISSION_GRANTED)
                            startVoiceRecording()
                        else micPermission.launch(Manifest.permission.RECORD_AUDIO)
                    },
                    micRelease={finishVoiceRecording()},micCancel={cancelVoiceRecording()},
                    playVoice=::toggleVoicePlayback,markVisible=::markVisible,
                    chatVisibility={chatScreenVisible=it},
                    connect={
                        if(!hasBluetoothPermission()) {
                            if(Build.VERSION.SDK_INT>=31)bluetoothPermission.launch(Manifest.permission.BLUETOOTH_CONNECT)
                        } else if(!pairingSaved)showKey=true
                        else if(trustedAddress==null)startSystemPairing(systemPicker::launch)
                        else reconnectIfPermitted()
                    },
                    disconnect={link?.stop();state="Disconnected";radioLink=null},
                    setRadioMode=::chooseRadioMode,
                    batterySettings={
                        val intent=Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                            Uri.parse("package:$packageName"))
                        runCatching {startActivity(intent)}.onFailure {bleTrace("Battery settings unavailable")}
                    },
                    chooseNode={startSystemPairing(systemPicker::launch)},
                    forgetNode={
                        link?.stop();secure.clearNodeAddress();trustedAddress=null
                        state="BLE device forgotten; AES pairing key kept"
                    },editKey={showKey=true},
                    deleteForMe=::deleteForMe,deleteForEveryone=::deleteForEveryone,
                    editMessage=::editOutgoing,
                    copyMessage={ item ->
                        val clipboard=getSystemService(android.content.ClipboardManager::class.java)
                        clipboard.setPrimaryClip(android.content.ClipData.newPlainText("Message",item.text))
                    },
                    chooseAvatar={avatarPicker.launch("image/*")},
                    clearAvatar={
                        avatarBytes=null;notifications.setContact(contactName,null)
                        historyWorker.execute {runCatching {history.clearAvatar()}}
                    },
                    renameContact={value ->
                        contactName=value.take(36);notifications.setContact(contactName,avatarBytes)
                        prefs.edit().putString("contact_name",contactName).apply()
                    },
                    changeDark={dark=it;prefs.edit().putBoolean("dark",it).apply()},
                    changeFastPhotos={
                        fastPhotos=it;getSharedPreferences("offgrid_media",MODE_PRIVATE)
                            .edit().putBoolean("fast_photos",it).apply()
                    },
                    changeNotifications={
                        notificationsEnabled=it;prefs.edit().putBoolean("notifications",it).apply()
                        if(it && Build.VERSION.SDK_INT>=33 && !notifications.canNotify())
                            notificationsPermission.launch(Manifest.permission.POST_NOTIFICATIONS)
                    },
                    changePrivatePreview={notificationPreview=it;prefs.edit().putBoolean("previews",it).apply()},
                    changeBackground=::toggleBackground,
                    changeAccessibleVoiceTap={
                        accessibleVoiceTap=it;prefs.edit().putBoolean("accessible_voice_tap",it).apply()
                    },
                    clearHistory={
                        items.clear();playedReceipts.clear();hiddenMessageIds.clear()
                        historyWorker.execute { history.clear() }
                    },retry=::retryFailed
                )
            )
            if(powerApprovalRequest!=null) {
                val requested=powerApprovalRequest!!
                AlertDialog(
                    onDismissRequest={declineHighPower(requested)},
                    title={Text("LoRa 18 dBm confirmation")},
                    text={Text("Fast and Max Range use nominal +18 dBm. Confirm BOTH ESP32 radio boards have suitable antennas, stable 3.3 V supplies, and that this transmit power is permitted where you operate. The receiving board will follow the authenticated RF switch automatically.")},
                    confirmButton={TextButton(onClick={authorizeHighPower(requested)}){Text("Confirm both boards")}},
                    dismissButton={TextButton(onClick={declineHighPower(requested)}){
                        Text(if(requested=="MEDIA")"Use current mode" else "Cancel")
                    }}
                )
            }
            if(showKey) {
                AlertDialog(onDismissRequest={if(pairingSaved)showKey=false},
                    title={Text("Secure node pairing")},
                    text={Column(verticalArrangement=Arrangement.spacedBy(10.dp)) {
                        Text("Enter the existing 43-character Base64URL key shared by both ESP32s. This isn't a new BLE PIN.")
                        OutlinedTextField(value=keyInput,onValueChange={keyInput=it.trim()},singleLine=true,
                            label={Text("OffGrid encryption key")},visualTransformation=PasswordVisualTransformation())
                        Text("Use the key shown in the trusted web interface. Do not share it.",
                            style=MaterialTheme.typography.bodySmall)
                    }},confirmButton={TextButton(onClick={
                        try {
                            val raw=OgmCodec.key(keyInput)
                            secure.savePairingKey(raw);raw.fill(0);pairingSaved=true;keyInput="";showKey=false
                            if(trustedAddress!=null)reconnectIfPermitted()
                            else if(hasBluetoothPermission())startSystemPairing(systemPicker::launch)
                            else if(Build.VERSION.SDK_INT>=31)bluetoothPermission.launch(Manifest.permission.BLUETOOTH_CONNECT)
                        } catch(_:Exception){state="Pairing key must be exactly 43 Base64URL characters"}
                    }){Text("Save & connect")}},
                    dismissButton=if(pairingSaved) {{TextButton(onClick={showKey=false}){Text("Cancel")}}} else null
                )
            }
        }
    }
}
