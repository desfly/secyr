package ua.homeguard.s3.provisioning

import android.content.Context
import android.os.Build
import android.security.KeyPairGeneratorSpec
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.util.Base64
import androidx.annotation.RequiresApi
import java.math.BigInteger
import java.security.KeyPairGenerator
import java.security.KeyStore
import java.security.SecureRandom
import java.util.Calendar
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.IvParameterSpec
import javax.crypto.spec.SecretKeySpec
import javax.security.auth.x500.X500Principal

class SecureTokenStore(context: Context) {
    private val appContext = context.applicationContext
    private val preferences = appContext.getSharedPreferences("homeguard_secure_values", Context.MODE_PRIVATE)
    private val keyAlias = "homeguard-s3-app-secrets-v1"
    private val legacyWrappedKeyName = "__legacy_wrapped_aes_key"

    fun put(name: String, value: String) {
        if (value.isEmpty()) {
            preferences.edit().remove(name).apply()
            return
        }
        val encoded = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            encryptModern(value)
        } else {
            encryptLegacy(value)
        }
        preferences.edit().putString(name, encoded).apply()
    }

    fun get(name: String): String {
        val encoded = preferences.getString(name, null) ?: return ""
        return runCatching {
            when {
                encoded.startsWith("v2:") && Build.VERSION.SDK_INT >= Build.VERSION_CODES.M -> decryptModern(encoded.removePrefix("v2:"))
                encoded.startsWith("v1:") -> decryptLegacy(encoded.removePrefix("v1:"))
                Build.VERSION.SDK_INT >= Build.VERSION_CODES.M -> decryptModern(encoded)
                else -> ""
            }
        }.getOrDefault("")
    }

    @RequiresApi(Build.VERSION_CODES.M)
    private fun encryptModern(value: String): String {
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.ENCRYPT_MODE, modernSecretKey())
        val encrypted = cipher.doFinal(value.toByteArray(Charsets.UTF_8))
        return "v2:" + Base64.encodeToString(cipher.iv + encrypted, Base64.NO_WRAP)
    }

    @RequiresApi(Build.VERSION_CODES.M)
    private fun decryptModern(encoded: String): String {
        val payload = Base64.decode(encoded, Base64.NO_WRAP)
        require(payload.size > 12)
        val iv = payload.copyOfRange(0, 12)
        val ciphertext = payload.copyOfRange(12, payload.size)
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.DECRYPT_MODE, modernSecretKey(), GCMParameterSpec(128, iv))
        return cipher.doFinal(ciphertext).toString(Charsets.UTF_8)
    }

    @RequiresApi(Build.VERSION_CODES.M)
    private fun modernSecretKey(): SecretKey {
        val store = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }
        (store.getKey(keyAlias, null) as? SecretKey)?.let { return it }
        val generator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore")
        generator.init(
            KeyGenParameterSpec.Builder(
                keyAlias,
                KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT
            ).setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                .setKeySize(256)
                .build()
        )
        return generator.generateKey()
    }

    private fun encryptLegacy(value: String): String {
        val key = legacySecretKey()
        val cipher = Cipher.getInstance("AES/CBC/PKCS5Padding")
        cipher.init(Cipher.ENCRYPT_MODE, key)
        val encrypted = cipher.doFinal(value.toByteArray(Charsets.UTF_8))
        return "v1:" + Base64.encodeToString(cipher.iv + encrypted, Base64.NO_WRAP)
    }

    private fun decryptLegacy(encoded: String): String {
        val payload = Base64.decode(encoded, Base64.NO_WRAP)
        require(payload.size > 16)
        val iv = payload.copyOfRange(0, 16)
        val ciphertext = payload.copyOfRange(16, payload.size)
        val cipher = Cipher.getInstance("AES/CBC/PKCS5Padding")
        cipher.init(Cipher.DECRYPT_MODE, legacySecretKey(), IvParameterSpec(iv))
        return cipher.doFinal(ciphertext).toString(Charsets.UTF_8)
    }

    private fun legacySecretKey(): SecretKey {
        val wrapped = preferences.getString(legacyWrappedKeyName, null)
        if (wrapped != null) {
            val cipher = Cipher.getInstance("RSA/ECB/PKCS1Padding")
            cipher.init(Cipher.DECRYPT_MODE, legacyPrivateKey())
            return SecretKeySpec(cipher.doFinal(Base64.decode(wrapped, Base64.NO_WRAP)), "AES")
        }

        val raw = ByteArray(32).also(SecureRandom()::nextBytes)
        val cipher = Cipher.getInstance("RSA/ECB/PKCS1Padding")
        cipher.init(Cipher.ENCRYPT_MODE, legacyPublicKey())
        preferences.edit()
            .putString(legacyWrappedKeyName, Base64.encodeToString(cipher.doFinal(raw), Base64.NO_WRAP))
            .commit()
        return SecretKeySpec(raw, "AES")
    }

    private fun legacyPublicKey() = legacyKeyStore().getCertificate(keyAlias).publicKey

    private fun legacyPrivateKey() = legacyKeyStore().getKey(keyAlias, null)

    private fun legacyKeyStore(): KeyStore {
        val store = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }
        if (!store.containsAlias(keyAlias)) {
            val start = Calendar.getInstance()
            val end = Calendar.getInstance().apply { add(Calendar.YEAR, 25) }
            val spec = KeyPairGeneratorSpec.Builder(appContext)
                .setAlias(keyAlias)
                .setSubject(X500Principal("CN=HomeGuard-S3,O=HomeGuard"))
                .setSerialNumber(BigInteger.ONE)
                .setStartDate(start.time)
                .setEndDate(end.time)
                .build()
            KeyPairGenerator.getInstance("RSA", "AndroidKeyStore").apply { initialize(spec) }.generateKeyPair()
        }
        return store
    }
}
