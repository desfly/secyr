package ua.homeguard.s3.storage

import android.content.Context
import ua.homeguard.s3.provisioning.SecureTokenStore

class FirstRunAuthStore(context: Context) {
    private val preferences = context.applicationContext.getSharedPreferences("homeguard_first_run", Context.MODE_PRIVATE)
    private val secure = SecureTokenStore(context)

    val completed: Boolean
        get() = preferences.getBoolean(KEY_COMPLETED, false)

    val username: String
        get() = preferences.getString(KEY_USERNAME, "").orEmpty()

    val credential: String
        get() = secure.get(KEY_CREDENTIAL)

    fun remember(username: String, credential: String) {
        preferences.edit()
            .putBoolean(KEY_COMPLETED, true)
            .putString(KEY_USERNAME, username.trim())
            .apply()
        secure.put(KEY_CREDENTIAL, credential)
    }

    fun reset() {
        preferences.edit().clear().apply()
        secure.put(KEY_CREDENTIAL, "")
    }

    companion object {
        private const val KEY_COMPLETED = "completed"
        private const val KEY_USERNAME = "username"
        private const val KEY_CREDENTIAL = "first_run_credential"
    }
}
