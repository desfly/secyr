package ua.homeguard.s3.notifications

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.os.Build
import android.os.IBinder
import androidx.core.app.NotificationCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.launch
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import ua.homeguard.s3.network.ble.BleHomeGuardClient
import ua.homeguard.s3.MainActivity
import ua.homeguard.s3.model.SystemEventRecord
import ua.homeguard.s3.model.SystemMode
import ua.homeguard.s3.network.MonitoringRuntime

class MonitoringService : Service() {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private lateinit var runtime: MonitoringRuntime

    override fun onCreate() {
        super.onCreate()
        val manager = getSystemService(NotificationManager::class.java)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) manager.createNotificationChannel(
            NotificationChannel("homeguard_monitor", "HomeGuard · Моніторинг", NotificationManager.IMPORTANCE_LOW)
        )
        val open = PendingIntent.getActivity(this, 0, Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE)
        startForeground(2147483646, NotificationCompat.Builder(this, "homeguard_monitor")
            .setSmallIcon(android.R.drawable.ic_dialog_info)
            .setContentTitle("HomeGuard · Моніторинг активний")
            .setContentText("Прийом подій від контролера")
            .setContentIntent(open).setOngoing(true).build())
        runtime = MonitoringRuntime.get(this)
        val notifications = HomeGuardNotifications(this).apply { createChannels() }
        scope.launch {
            runtime.telemetry.liveEvents().collect { event ->
                runtime.eventHistory.append(event)
                notifications.notify(event, runtime.settings.settings.value)
            }
        }
        scope.launch {
            var alarm = false
            runtime.telemetry.snapshots().collect { snapshot ->
                if (snapshot.mode == SystemMode.ALARM && !alarm) notifications.notify(
                    SystemEventRecord(snapshot.sequence, System.currentTimeMillis(), "ALARM", 0, 1),
                    runtime.settings.settings.value)
                alarm = snapshot.mode == SystemMode.ALARM
            }
        }
        runtime.start()
        // Restore authorization after a service/process restart. The visible
        // screen and this service share the same controller/session owner.
        scope.launch {
            while (isActive) {
                delay(30_000L)
                if (runtime.commands.hasLocalSession() ||
                    runtime.commands.bleState().value == BleHomeGuardClient.State.READY) continue
                val id = runtime.settings.settings.value.deviceId
                val saved = runtime.settings.savedLogin(id) ?: continue
                runCatching { runtime.commands.login(saved.actor, saved.pin) }
            }
        }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int = START_STICKY
    override fun onBind(intent: Intent?): IBinder? = null
    override fun onDestroy() {
        scope.cancel()
        if (::runtime.isInitialized) runtime.stop()
        super.onDestroy()
    }
}
