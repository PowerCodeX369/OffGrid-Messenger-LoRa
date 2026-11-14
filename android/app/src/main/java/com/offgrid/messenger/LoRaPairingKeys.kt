package com.offgrid.messenger

import android.content.Context
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.util.Base64
import java.security.KeyStore
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

/** Keys are encrypted with a non-exportable AndroidKeyStore AES-GCM key.
 * A separate radio NVS pairing key must be provisioned once over physical USB.
 * Never log or export the BLE key material in diagnostics. */
class LoRaPairingKeys(context:Context) {
    private val prefs=context.getSharedPreferences("og_lora_ble_secrets_v1",Context.MODE_PRIVATE)
    private val alias="com.offgrid.messenger.LoRaBLE.GCM.v1"
    private fun secret():SecretKey {
        val store=KeyStore.getInstance("AndroidKeyStore").apply{load(null)}
        (store.getKey(alias,null) as? SecretKey)?.let{return it}
        val gen=KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES,"AndroidKeyStore")
        gen.init(KeyGenParameterSpec.Builder(alias,KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT)
            .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
            .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
            .setKeySize(256).build())
        return gen.generateKey()
    }
    fun save(address:String,value:ByteArray) {
        require(value.size==32)
        val cipher=Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.ENCRYPT_MODE,secret())
        cipher.updateAAD(address.toByteArray(Charsets.UTF_8))
        val encrypted=cipher.doFinal(value)
        prefs.edit().putString(address,Base64.encodeToString(cipher.iv+encrypted,Base64.NO_WRAP)).apply()
    }
    fun load(address:String):ByteArray? {
        val saved=prefs.getString(address,null) ?: return null
        return runCatching {
            val bytes=Base64.decode(saved,Base64.NO_WRAP)
            require(bytes.size >= 12+16+32)
            val cipher=Cipher.getInstance("AES/GCM/NoPadding")
            cipher.init(Cipher.DECRYPT_MODE,secret(),GCMParameterSpec(128,bytes.copyOfRange(0,12)))
            cipher.updateAAD(address.toByteArray(Charsets.UTF_8))
            cipher.doFinal(bytes.copyOfRange(12,bytes.size)).also{require(it.size==32)}
        }.getOrNull()
    }
    fun forget(address:String) {prefs.edit().remove(address).apply()}
    companion object {
        fun parse(raw:String):ByteArray = LoRaMobileCodec.parsePairingKey(raw)
    }
}
