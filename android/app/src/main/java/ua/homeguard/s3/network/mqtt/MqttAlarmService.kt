package ua.homeguard.s3.network.mqtt

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.os.IBinder
import androidx.core.app.NotificationCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import ua.homeguard.s3.MainActivity
import ua.homeguard.s3.notifications.HomeGuardNotifications
import ua.homeguard.s3.model.SystemEventRecord
import ua.homeguard.s3.storage.SettingsStore

/**
 * Keeps the MQTT alarm subscription alive when the MyFist activity is closed.
 * This service owns no command path; it only listens for controller events.
 */
class MqttAlarmService : Service() {
    companion object {
        private const val CHANNEL = "homeguard_mqtt_watch"
        private const val NOTIFICATION_ID = 7301
    }

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private lateinit var settings: SettingsStore
    private lateinit var client: MqttRuntimeClient
    private lateinit var notifications: HomeGuardNotifications

    override fun onCreate() {
        super.onCreate()
        settings = SettingsStore(this)
        client = MqttRuntimeClient(scope)
        notifications = HomeGuardNotifications(this)
        notifications.createChannels()
        createServiceChannel()
        startForeground(NOTIFICATION_ID, serviceNotification())
        scope.launch {
            client.events().collect { json ->
                if (json.optString("event", "").equals("alarm", ignoreCase = true)) {
                    notifications.notify(
                        SystemEventRecord(
                            sequence = json.optLong("seq", System.currentTimeMillis()),
                            timestampMs = json.optLong("ts", System.currentTimeMillis()),
                            event = "ALARM",
                            sourceId = json.optInt("source", 0),
                            value = json.optInt("value", 1),
                        ),
                        settings.settings.value,
                    )
                }
            }
        }
        connect()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        connect()
        return START_STICKY
    }

    override fun onDestroy() {
        client.stop()
        scope.cancel()
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null

    private fun connect() {
        val current = settings.settings.value
        val password = settings.mqttPassword()
        if (current.deviceId.isBlank() || current.mqttBrokerUri.isBlank() || password.isBlank()) return
        client.start(
            MqttRuntimeClient.Config(
                brokerUri = current.mqttBrokerUri,
                username = current.mqttUsername,
                password = password,
                deviceId = current.deviceId,
                clientId = "android-alarm-" + current.deviceId,
            ),
        )
    }

    private fun createServiceChannel() {
        val manager = getSystemService(NotificationManager::class.java)
        manager.createNotificationChannel(
            NotificationChannel(CHANNEL, "HomeGuard MQTT", NotificationManager.IMPORTANCE_LOW).apply {
                setSound(null, null)
                enableVibration(false)
            },
        )
    }

    private fun serviceNotification() = NotificationCompat.Builder(this, CHANNEL)
        .setSmallIcon(android.R.drawable.ic_lock_idle_alarm)
        .setContentTitle("MyFist")
        .setContentText("Контроль тривог активний")
        .setOngoing(true)
        .setSilent(true)
        .setContentIntent(
            PendingIntent.getActivity(
                this,
                0,
                Intent(this, MainActivity::class.java),
                PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
            ),
        )
        .build()
}
