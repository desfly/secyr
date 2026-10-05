package ua.homeguard.s3.network.mqtt

import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.util.Base64
import java.security.KeyPairGenerator
import java.security.KeyStore
import java.security.Signature
import java.security.spec.ECGenParameterSpec

/**
 * Owns the per-device MQTT command signing key.
 *
 * The private P-256 key never leaves Android Keystore. The public key is exported
 * as SubjectPublicKeyInfo PEM for the ESP /api/v1/cloud/trust endpoint.
 */
class MqttCommandSigner(private val deviceId: String) {
    init {
        require(deviceId.isNotBlank()) { "deviceId is required" }
    }

    private val alias = "homeguard-mqtt-command-v1-" + deviceId

    fun publicKeyPem(): String {
        val key = keyPair().certificate.publicKey
        val body = Base64.encodeToString(key.encoded, Base64.NO_WRAP)
        return buildString {
            append("-----BEGIN PUBLIC KEY-----\n")
            body.chunked(64).forEach { append(it).append('\n') }
            append("-----END PUBLIC KEY-----\n")
        }
    }

    fun signCanonical(canonical: String): String {
        require(canonical.isNotEmpty()) { "canonical envelope is required" }
        val signature = Signature.getInstance("SHA256withECDSA")
        signature.initSign(keyPair().privateKey)
        signature.update(canonical.toByteArray(Charsets.UTF_8))
        return signature.sign().joinToString(separator = "") { "%02x".format(it.toInt() and 0xff) }
    }

    private fun keyPair(): KeyStore.PrivateKeyEntry {
        val store = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }
        (store.getEntry(alias, null) as? KeyStore.PrivateKeyEntry)?.let { return it }

        val generator = KeyPairGenerator.getInstance(KeyProperties.KEY_ALGORITHM_EC, "AndroidKeyStore")
        generator.initialize(
            KeyGenParameterSpec.Builder(alias, KeyProperties.PURPOSE_SIGN)
                .setAlgorithmParameterSpec(ECGenParameterSpec("secp256r1"))
                .setDigests(KeyProperties.DIGEST_SHA256)
                .build(),
        )
        generator.generateKeyPair()
        return store.getEntry(alias, null) as KeyStore.PrivateKeyEntry
    }
}
