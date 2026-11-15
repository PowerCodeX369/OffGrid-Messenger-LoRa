package com.offgrid.messenger

/** Authenticated SX1278 peer telemetry forwarded by the existing LoRa BLE service.
 * RSSI is the remote LoRa radio link, NOT the phone's BLE RSSI. */
data class RadioLink(
    val peer: String,
    val rssiDbm: Int?,
    val phy: String,
    val phyMode: String,
    val rateMode: String,
    val changes: Long?,
    val normalPeerAgeMs: Long?,
    val activeProfile: String = "UNKNOWN"
) {
    val hasPeer: Boolean get() = peer != "BROADCAST" && peer != "NONE"
    val signalLabel: String get() = when {
        normalPeerAgeMs != null && (normalPeerAgeMs < 0 || normalPeerAgeMs >= 30_000) -> "LoRa peer silent"
        else -> rssiDbm?.let { "$it dBm" } ?: "No peer signal"
    }
    val phyLabel: String get() = when (if(rateMode=="AUTO")activeProfile else rateMode) {
        "FAST" -> "SF7 / BW500"
        "BALANCED" -> "SF7 / BW250"
        "RANGE" -> "SF9 / BW125"
        "ULTRA" -> "SF12 / BW125"
        "AUTO" -> "SX1278 LoRa adaptive"
        else -> "SX1278 LoRa"
    }
    val modeLabel: String get() = when (rateMode) {
        "FAST" -> "Fast"
        "BALANCED" -> "Balanced"
        "RANGE" -> "Max Range"
        "ULTRA" -> "Ultra Long Range"
        "AUTO" -> "Auto · local RSSI $rssiTarget"
        else -> "Unknown"
    }
    /** Informational target; only a verified firmware PROFILE|ACTIVE changes RF. */
    val rssiTarget: String get() = when {
        rssiDbm == null -> "Waiting for authenticated LoRa RSSI"
        rssiDbm > -100 -> "Balanced"
        rssiDbm >= -130 -> "Max Range"
        else -> "Ultra"
    }
    val summary: String get() = "$signalLabel · $phyLabel · $modeLabel" +
        if (rateMode == "AUTO") " · local RSSI band: $rssiTarget" else ""

    companion object {
        val choices: List<String> = listOf("AUTO", "FAST", "BALANCED", "RANGE", "ULTRA")
        fun command(mode: String): String? = when (mode) {
            "AUTO" -> "AUTO ON"
            "FAST" -> "PROFILE LOW"
            "BALANCED" -> "PROFILE BALANCED"
            "RANGE" -> "PROFILE MAX"
            "ULTRA" -> "PROFILE ULTRA"
            else -> null
        }
        fun requiresHighPower(mode: String): Boolean = mode in setOf("AUTO", "FAST", "RANGE", "ULTRA")
        fun parse(line: String): RadioLink? {
            if (!line.startsWith("LINK|")) return null
            val fields = line.split('|')
            if (fields.size < 3 || fields[1].isBlank()) return null
            val values = fields.drop(2).mapNotNull { field ->
                val i = field.indexOf('=')
                if (i <= 0 || i == field.lastIndex) null else field.substring(0, i) to field.substring(i + 1)
            }.toMap()
            val peer = fields[1]
            val age = (values["PEER_AGE_MS"] ?: values["REGAGE"])?.toLongOrNull()
            val fresh = (values["RFHEARD"] ?: values["RFOK"]) != "0" && (age == null || age in 0..29_999)
            val dbm = values["RSSI"]?.toIntOrNull()?.takeIf {
                fresh && peer != "BROADCAST" && peer != "NONE" && it in -160..-1
            }
            // The LoRa firmware sends PROFILE=LOW/BALANCED/MAX and AUTO=0/1.
            // Never interpret legacy ESP-NOW PHY=0.25/0.5 or MODE=LR250 as LoRa speeds.
            val mode = when {
                values["AUTO"] == "1" -> "AUTO"
                values["PROFILE"] == "LOW" -> "FAST"
                values["PROFILE"] == "BALANCED" -> "BALANCED"
                values["PROFILE"] == "MAX" -> "RANGE"
                values["PROFILE"] == "ULTRA" -> "ULTRA"
                else -> "UNKNOWN"
            }
            return RadioLink(peer, dbm, values["PHY"] ?: "", values["PHYMODE"] ?: "",
                mode, values["PROFILE_CHANGES"]?.toLongOrNull() ?: values["CHANGES"]?.toLongOrNull(),
                age,
                when(values["PROFILE"]){"LOW"->"FAST";"BALANCED"->"BALANCED";"MAX"->"RANGE";"ULTRA"->"ULTRA";else->"UNKNOWN"})
        }
    }
}
