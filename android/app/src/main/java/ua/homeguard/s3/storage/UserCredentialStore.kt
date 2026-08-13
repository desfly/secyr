package ua.homeguard.s3.storage

import android.content.Context
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import ua.homeguard.s3.provisioning.SecureTokenStore

data class SavedUserCredentials(
    val username: String,
    val password: String,
)

class UserCredentialStore(context: Context) {
    private val preferences = context.applicationContext.getSharedPreferences("homeguard_user_credentials", Context.MODE_PRIVATE)
    private val secure = SecureTokenStore(context)
    private val mutable = MutableStateFlow(load())
    val credentials: StateFlow<SavedUserCredentials?> = mutable

    fun save(username: String, password: String) {
        val normalized = username.trim()
        require(normalized.isNotEmpty()) { "Username is required" }
        require(password.isNotEmpty()) { "Password is required" }
        preferences.edit().putString(KEY_USERNAME, normalized).apply()
        secure.put(KEY_PASSWORD, password)
        mutable.value = SavedUserCredentials(normalized, password)
    }

    fun clear() {
        preferences.edit().remove(KEY_USERNAME).apply()
        secure.put(KEY_PASSWORD, "")
        mutable.value = null
    }

    fun isConfigured(): Boolean = mutable.value != null

    private fun load(): SavedUserCredentials? {
        val username = preferences.getString(KEY_USERNAME, "").orEmpty().trim()
        val password = secure.get(KEY_PASSWORD)
        return if (username.isBlank() || password.isBlank()) null else SavedUserCredentials(username, password)
    }

    private companion object {
        const val KEY_USERNAME = "first_run_username"
        const val KEY_PASSWORD = "first_run_password"
    }
}
