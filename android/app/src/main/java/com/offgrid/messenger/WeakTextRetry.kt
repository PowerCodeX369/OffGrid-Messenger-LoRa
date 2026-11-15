package com.offgrid.messenger

/** Bounded *transmission frequency*, not a guarantee of RF delivery.
 * IDs identify an application message, NOT a radio transfer. The sender
 * re-seals each repeat with a fresh AES-GCM nonce and LoRa transfer ID.
 * Persist the original message using EncryptedHistory; no plaintext is
 * written to this scheduler or to SharedPreferences.
 */
class WeakTextRetry {
    private data class Slot(var attempts:Int=0, var nextAt:Long=0L)
    private val slots=mutableMapOf<String, Slot>()
    fun due(id:String, now:Long, freshPeer:Boolean):Boolean = freshPeer &&
        (slots[id]?.nextAt ?: 0L)<=now
    fun attempt(id:String,now:Long) {
        val slot=slots.getOrPut(id){Slot()}
        slot.attempts=(slot.attempts+1).coerceAtMost(16)
        slot.nextAt=now+delay(slot.attempts)+jitter(id)
    }
    fun postpone(id:String,now:Long){
        val slot=slots.getOrPut(id){Slot()}
        slot.nextAt=maxOf(slot.nextAt,now+delay(slot.attempts.coerceAtLeast(1))+jitter(id))
    }
    fun holdForReceipt(id:String,now:Long,isUltra:Boolean){
        val slot=slots.getOrPut(id){Slot()}
        slot.nextAt=maxOf(slot.nextAt,now+receiptWait(isUltra))
    }
    fun acknowledged(id:String){slots.remove(id)}
    fun reset(){slots.clear()}
    fun pendingCount():Int=slots.size
    companion object {
        /* Exponential backoff capped at five minutes. Application policy also
         * requires a *fresh authenticated RF peer* and limits transmissions
         * to one outstanding text transfer. LoRa duty-cycle rules still apply. */
        fun delay(attempt:Int):Long {
            val step=(attempt.coerceIn(1,6)-1)
            return minOf(300_000L,15_000L*(1L shl step))
        }
        // Randomized message IDs provide stable per-message timing jitter, so
        // two peers retrying simultaneously are less likely to collide again.
        fun jitter(id:String):Long=(id.hashCode().toLong() and 0x7FFFFFFFL)%5_001L
        fun receiptWait(isUltra:Boolean):Long=if(isUltra)240_000L else 90_000L
    }
}
