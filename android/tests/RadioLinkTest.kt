import com.offgrid.messenger.RadioLink

fun main() {
    val fast=RadioLink.parse("LINK|14:63:93:C2:F3:3C|RSSI=-39|PHY=0|MODE=LORA|PROFILE=LOW|AUTO=0|PROFILE_CHANGES=4")!!
    check(fast.rateMode=="FAST" && fast.modeLabel=="Fast" && fast.phyLabel=="SF7 / BW500")
    check(fast.summary.contains("-39 dBm") && !fast.summary.contains("LR 500 kbps"))
    check(fast.changes==4L)
    val balanced=RadioLink.parse("LINK|14:63:93:C2:F3:3C|RSSI=-49|PHY=0|MODE=LORA|PROFILE=BALANCED|AUTO=0")!!
    check(balanced.rateMode=="BALANCED" && balanced.phyLabel=="SF7 / BW250")
    val range=RadioLink.parse("LINK|14:63:93:C2:F3:3C|RSSI=-117|PHY=0|PROFILE=MAX|AUTO=0")!!
    check(range.rateMode=="RANGE" && range.phyLabel=="SF9 / BW125")
    val auto=RadioLink.parse("LINK|14:63:93:C2:F3:3C|RSSI=-85|PHY=0|PROFILE=LOW|AUTO=1")!!
    check(auto.rateMode=="AUTO" && auto.modeLabel=="Auto (adaptive)" && auto.phyLabel=="SF7 / BW500")
    val legacy=RadioLink.parse("LINK|14:63:93:C2:F3:3C|RSSI=-58|PHY=0.25|MODE=LR250|PHYMODE=LR250")!!
    check(legacy.rateMode=="UNKNOWN" && legacy.phyLabel=="SX1278 LoRa")
    val lost=RadioLink.parse("LINK|AA:BB:CC:DD:EE:FF|RSSI=-58|PROFILE=LOW|REGAGE=20000")!!
    check(lost.signalLabel=="Peer not heard")
    val stale=RadioLink.parse("LINK|AA:BB:CC:DD:EE:FF|RSSI=-98|PEER_AGE_MS=31000|RFHEARD=0|PROFILE=LOW|AUTO=1")!!
    check(stale.rssiDbm==null && stale.signalLabel=="LoRa peer silent")
    val freshWeak=RadioLink.parse("LINK|AA:BB:CC:DD:EE:FF|RSSI=-91|PEER_AGE_MS=2300|RFHEARD=1|PROFILE=MAX|AUTO=1")!!
    check(freshWeak.rssiDbm==-91 && freshWeak.rateMode=="AUTO" && freshWeak.activeProfile=="RANGE")
    val oldLinkWithoutAge=RadioLink.parse("LINK|14:63:93:C2:F3:3C|RSSI=-52|PROFILE=BALANCED|AUTO=0")!!
    check(oldLinkWithoutAge.rssiDbm==-52) // backward compatibility with rc.4
    val absent=RadioLink.parse("LINK|NONE|RSSI=0|PROFILE=BALANCED")!!
    check(absent.rssiDbm==null)
    check(RadioLink.parse("READY|node")==null)
    check(RadioLink.parse("LINK||RSSI=-50")==null)
    check(RadioLink.choices==listOf("AUTO","FAST","BALANCED","RANGE"))
    check(RadioLink.command("FAST")=="PROFILE LOW")
    check(RadioLink.command("BALANCED")=="PROFILE BALANCED")
    check(RadioLink.command("RANGE")=="PROFILE MAX")
    check(RadioLink.command("AUTO")=="AUTO ON")
    check(RadioLink.command("LR250")==null && RadioLink.command("LR500")==null)
    check(!RadioLink.requiresHighPower("BALANCED"))
    check(RadioLink.requiresHighPower("FAST") && RadioLink.requiresHighPower("RANGE") && RadioLink.requiresHighPower("AUTO"))
    println("PASS: actual SX1278 link telemetry, three radio profiles + Auto, no ESP-NOW speeds")
}
