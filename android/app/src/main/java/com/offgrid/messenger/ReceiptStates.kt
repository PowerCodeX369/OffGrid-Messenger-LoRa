package com.offgrid.messenger

/** Display never upgrades a receipt without an authenticated event.
 * Radio queued is NOT remote delivery; the second status requires OGM4 DELIVERED.
 * Voice ticks turn blue on READ; only the microphone turns blue on PLAYED.
 */
enum class ReceiptVisual { WAITING, SENT, DELIVERED, READ, FAILED }
object ReceiptStates {
    fun visual(status: String, @Suppress("UNUSED_PARAMETER") kind: Int): ReceiptVisual = when {
        status == "failed" -> ReceiptVisual.FAILED
        status == "read" || status == "played" -> ReceiptVisual.READ
        status == "delivered" -> ReceiptVisual.DELIVERED
        status == "sent" -> ReceiptVisual.SENT
        else -> ReceiptVisual.WAITING
    }
    private fun rank(status: String): Int = when(status) {
        "sending" -> 0; "sent" -> 1; "delivered" -> 2; "read" -> 3; "played" -> 4; else -> -1
    }
    fun advance(previous: String, incoming: String): String = when {
        // A temporarily unusable LoRa link is recoverable. Do not show a
        // permanent failure or erase an authenticated delivery/read receipt.
        incoming == "waiting" -> if(rank(previous)>=2) previous else "waiting"
        incoming == "failed" -> if(rank(previous)>=2) previous else "failed"
        previous == "failed" && incoming == "sending" -> "sending"
        previous == "failed" && rank(incoming)<2 -> previous
        rank(incoming)>rank(previous) -> incoming
        else -> previous
    }
}
