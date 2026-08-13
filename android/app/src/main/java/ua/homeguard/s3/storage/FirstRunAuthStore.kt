package ua.homeguard.s3.storage

import android.content.Context

class FirstRunAuthStore(context: Context) {
    private val preferences = context.applicationContext.getSharedPreferences("homeguard_first_run", Context.MODE_PRIVATE)

    val completed: Boolean
        get() = preferences.getBoolean(KEY_COMPLETED, false)

    val username: String
        get() = preferences.getString(KEY_USERNAME, "").orEmpty()

    fun remember(username: String) {
        preferences.edit()
            .putBoolean(KEY_COMPLETED, true)
            .putString(KEY_USERNAME, username.trim())
            .apply()
    }

    fun reset() {
        preferences.edit().clear().apply()
    }

    companion object {
        private const val KEY_COMPLETED = "completed"
        private const val KEY_USERNAME = "username"
    }
}
