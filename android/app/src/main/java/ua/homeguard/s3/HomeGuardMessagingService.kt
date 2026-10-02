package ua.homeguard.s3

import android.content.Intent
import android.util.Log
import androidx.core.content.ContextCompat
import com.google.firebase.messaging.FirebaseMessagingService
import com.google.firebase.messaging.RemoteMessage

class HomeGuardMessagingService : FirebaseMessagingService() {
    companion object { private const val TAG = "HomeGuardPush" }

    override fun onNewToken(token: String) {
        super.onNewToken(token)
        // Registration with the authenticated HomeGuard backend is wired next.
        // Never print the token itself into shared logs.
        Log.i(TAG, "FCM token refreshed")
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
