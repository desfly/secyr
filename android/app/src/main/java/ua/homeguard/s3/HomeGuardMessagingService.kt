package ua.homeguard.s3

import android.content.Intent
import android.util.Log
import androidx.core.content.ContextCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch
import ua.homeguard.s3.push.PushTokenRegistrar
import com.google.firebase.messaging.FirebaseMessagingService
import com.google.firebase.messaging.RemoteMessage

class HomeGuardMessagingService : FirebaseMessagingService() {
    companion object { private const val TAG = "HomeGuardPush" }
    private val pushScope = CoroutineScope(SupervisorJob() + Dispatchers.IO)

    override fun onNewToken(token: String) {
        super.onNewToken(token)
        HomeGuardRuntime.ensureStarted(this)
        val settings = HomeGuardRuntime.settings.settings.value
        pushScope.launch {
            runCatching { PushTokenRegistrar.register(settings, token) }
                .onSuccess { Log.i(TAG, "FCM token registered") }
                .onFailure { error -> Log.w(TAG, "FCM token registration deferred: ${error.message}") }
        }
    }

    override fun onMessageReceived(message: RemoteMessage) {
        super.onMessageReceived(message)
        val data = message.data
        if (data["type"] != "HOMEGUARD_ALARM") return
        val event = data["event"]?.lowercase()
        if (event != "alarm" && event != "tamper") return

        val intent = Intent(this, AlarmMonitorService::class.java).apply {
            action = AlarmMonitorService.ACTION_PUSH_ALARM
            putExtra(AlarmMonitorService.EXTRA_DEVICE_ID, data["deviceId"].orEmpty())
            putExtra(AlarmMonitorService.EXTRA_EVENT, event)
        }
        try {
            ContextCompat.startForegroundService(this, intent)
        } catch (error: RuntimeException) {
            Log.e(TAG, "Unable to start alarm foreground service from push", error)
        }
    }
}
