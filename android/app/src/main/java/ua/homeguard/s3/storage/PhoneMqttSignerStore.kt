package ua.homeguard.s3.storage

import android.content.Context
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.util.Base64
import kotlinx.coroutines.flow.MutableStateFlow
import ua.homeguard.s3.network.mqtt.LocalCommandSigner
import java.security.KeyPairGenerator
import java.security.KeyStore
import java.security.MessageDigest
import java.security.PrivateKey
import java.security.spec.ECGenParameterSpec

/** Phone-only state. Enrollment must be confirmed by the authenticated controller first. */
class PhoneMqttSignerStore(context: Context) {
    private val preferences = context.applicationContext.getSharedPreferences("homeguard_phone_signers", Context.MODE_PRIVATE)
    val revision = MutableStateFlow(0L)

    companion object {
        private val gate = Any()
        private fun digest(value: ByteArray): String = MessageDigest.getInstance("SHA-256")
            .digest(value).joinToString("") { "%02x".format(it.toInt() and 255) }
    }

    private fun deviceKey(device: String): String {
        require(device.matches(Regex("[A-Za-z0-9_-]+")))
        return digest(device.toByteArray(Charsets.UTF_8))
    }

    private fun alias(device: String, actor: String): String {
        require(actor.isNotBlank() && actor.none { it.code < 32 || it.code == 127 })
        return "homeguard-phone-command-" + digest((deviceKey(device) + "\n" + actor).toByteArray(Charsets.UTF_8))
    }

    private fun keyStore(): KeyStore = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }

    /** Public key only. The private key is generated and retained inside AndroidKeyStore. */
    fun preparePublicKey(device: String, actor: String): String = synchronized(gate) {
        val name = alias(device, actor)
        if (!keyStore().containsAlias(name)) {
            KeyPairGenerator.getInstance(KeyProperties.KEY_ALGORITHM_EC, "AndroidKeyStore").apply {
                initialize(KeyGenParameterSpec.Builder(name, KeyProperties.PURPOSE_SIGN)
                    .setAlgorithmParameterSpec(ECGenParameterSpec("secp256r1"))
                    .setDigests(KeyProperties.DIGEST_SHA256).build())
            }.generateKeyPair()
        }
        val encoded = requireNotNull(keyStore().getCertificate(name)).publicKey.encoded
        "-----BEGIN PUBLIC KEY-----\n" + Base64.encodeToString(encoded, Base64.NO_WRAP)
            .chunked(64).joinToString("\n") + "\n-----END PUBLIC KEY-----\n"
    }

    /** Commit only after controller enrollment confirms this exact public key and replay floor. */
    fun activate(device: String, actor: String, epoch: Long, floor: Long, confirmedPublicKeyPem: String) = synchronized(gate) {
        require(epoch > 0 && floor >= 0 && floor < Long.MAX_VALUE)
        val name = alias(device, actor)
        val certificate = requireNotNull(keyStore().getCertificate(name)) { "Phone key missing" }
        val encoded = Base64.decode(confirmedPublicKeyPem.replace("-----BEGIN PUBLIC KEY-----", "")
            .replace("-----END PUBLIC KEY-----", "").filterNot(Char::isWhitespace), Base64.DEFAULT)
        require(certificate.publicKey.encoded.contentEquals(encoded)) { "Controller enrolled another key" }
        val deviceKey = deviceKey(device)
        val previous = preferences.getLong("counter_$deviceKey", 0L)
        val committed = preferences.edit().putLong("counter_$deviceKey", maxOf(previous, floor))
            .putString("actor_$deviceKey", actor).putLong("epoch_$deviceKey", epoch)
            .putString("key_$deviceKey", digest(encoded)).commit()
        check(committed) { "Phone enrollment persistence failed" }
        revision.value += 1
    }

    fun signer(device: String): LocalCommandSigner? = synchronized(gate) {
        if (device.isBlank()) return@synchronized null
        val id = deviceKey(device)
        val actor = preferences.getString("actor_$id", "").orEmpty()
        val epoch = preferences.getLong("epoch_$id", 0L)
        if (actor.isBlank() || epoch <= 0) return@synchronized null
        val name = alias(device, actor)
        val store = keyStore()
        val certificate = store.getCertificate(name) ?: return@synchronized null
        if (digest(certificate.publicKey.encoded) != preferences.getString("key_$id", "")) return@synchronized null
        if (store.getKey(name, null) !is PrivateKey) return@synchronized null
        LocalCommandSigner(device, actor, epoch,
            { requireNotNull(keyStore().getKey(name, null) as? PrivateKey) { "Phone signing key unavailable" } },
            { synchronized(gate) {
                check(preferences.getString("actor_$id", "") == actor &&
                    preferences.getLong("epoch_$id", 0L) == epoch &&
                    preferences.getString("key_$id", "") == digest(certificate.publicKey.encoded)) {
                    "Phone signing enrollment changed"
                }
                val previous = preferences.getLong("counter_$id", 0L)
                check(previous in 0 until Long.MAX_VALUE) { "Phone command counter exhausted" }
                val next = previous + 1
                check(preferences.edit().putLong("counter_$id", next).commit()) { "Phone command counter persistence failed" }
                next
            } },
        )
    }
}
