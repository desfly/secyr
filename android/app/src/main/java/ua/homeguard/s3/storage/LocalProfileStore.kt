package ua.homeguard.s3.storage

import android.content.Context
import ua.homeguard.s3.provisioning.SecureTokenStore

/**
 * Stores the first-run local profile without leaving the password in plaintext
 * SharedPreferences. The profile name is non-secret; the password is protected by
 * Android Keystore through SecureTokenStore.
 */
class LocalProfileStore(context: Context) {
    companion object {
        private const val PREFS = "myfist_profile"
        private const val KEY_REGISTERED = "registered"
        private const val KEY_NAME = "name"
        private const val LEGACY_PASSWORD = "password"
        private const val SECURE_PASSWORD = "profile_password"
    }

    private val preferences = context.applicationContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
    private val secure = SecureTokenStore(context.applicationContext)

    init {
        // One-time migration from builds that wrote the local profile password into
        // plaintext SharedPreferences.
        val legacy = preferences.getString(LEGACY_PASSWORD, null)
        if (!legacy.isNullOrEmpty()) {
            secure.put(SECURE_PASSWORD, legacy)
            preferences.edit().remove(LEGACY_PASSWORD).apply()
        }
    }

    fun isRegistered(): Boolean = preferences.getBoolean(KEY_REGISTERED, false)

    fun profileName(): String = preferences.getString(KEY_NAME, "").orEmpty()

    fun password(): String = secure.get(SECURE_PASSWORD)

    fun register(name: String, password: String) {
        val cleanName = name.trim().take(40)
        require(cleanName.isNotBlank()) { "Profile name is required" }
        require(password.length >= 4) { "Profile password is too short" }

        secure.put(SECURE_PASSWORD, password)
        preferences.edit()
            .putBoolean(KEY_REGISTERED, true)
            .putString(KEY_NAME, cleanName)
            .remove(LEGACY_PASSWORD)
            .apply()
    }
}
