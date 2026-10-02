package ua.homeguard.s3

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.media.AudioManager
import android.media.ToneGenerator
import android.os.Build
import android.os.IBinder
import android.os.VibrationEffect
import android.os.Vibrator
import androidx.core.app.NotificationCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.launch
import ua.homeguard.s3.model.SystemEventRecord
import ua.homeguard.s3.model.SystemMode
import ua.homeguard.s3.notifications.HomeGuardNotifications

class AlarmMonitorService : Service() {
    companion object {
        private const val CHANNEL_MONITOR = "homeguard_alarm_monitor"
        private const val MONITOR_NOTIFICATION_ID = 41001
        const val ACTION_PUSH_ALARM = "ua.homeguard.s3.action.PUSH_ALARM"
        const val EXTRA_DEVICE_ID = "device_id"
        const val EXTRA_EVENT = "event"
    }

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private lateinit var notifications: HomeGuardNotifications
    private var toneJob: Job? = null
    private var tone: ToneGenerator? = null
    private var alarmActive = false

    override fun onCreate() {
        super.onCreate()
        HomeGuardRuntime.ensureStarted(this)
        notifications = HomeGuardNotifications(this)
        notifications.createChannels()
        createMonitorChannel()
        startForeground(MONITOR_NOTIFICATION_ID, monitorNotification())

        scope.launch {
            HomeGuardRuntime.telemetry.snapshots().collect { snapshot ->
                if (snapshot.mode == SystemMode.ALARM) {
                    if (!alarmActive) {
                        alarmActive = true
                        startAlarmSignal()
                        notifications.notify(
                            SystemEventRecord(
                                sequence = snapshot.sequence,
                                timestampMs = System.currentTimeMillis(),
                                event = "ALARM",
                                sourceId = 0,
                                value = 1,
                            ),
                            HomeGuardRuntime.settings.settings.value,
                        )
                    }
                } else if (
                    snapshot.mode != SystemMode.ARMED_HOME &&
                    snapshot.mode != SystemMode.ARMED_AWAY
                ) {
                    stopAlarmSignal()
                }
            }
        }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_PUSH_ALARM && !alarmActive) {
            alarmActive = true
            startAlarmSignal()
            notifications.notify(
                SystemEventRecord(
                    sequence = System.currentTimeMillis(),
                    timestampMs = System.currentTimeMillis(),
                    event = intent.getStringExtra(EXTRA_EVENT)?.uppercase() ?: "ALARM",
                    sourceId = 0,
                    value = 1,
                ),
                HomeGuardRuntime.settings.settings.value,
            )
        }
        return START_STICKY
    }
    override fun onBind(intent: Intent?): IBinder? = null

    override fun onDestroy() {
        stopAlarmSignal()
        scope.cancel()
        super.onDestroy()
    }

    private fun createMonitorChannel() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return
        val manager = getSystemService(NotificationManager::class.java)
        manager.createNotificationChannel(
            NotificationChannel(
                CHANNEL_MONITOR,
                "HomeGuard monitoring",
                NotificationManager.IMPORTANCE_LOW,
            ).apply {
                description = "Фоновий контроль тривоги HomeGuard"
                setSound(null, null)
                enableVibration(false)
            }
        )
    }

    private fun monitorNotification() = NotificationCompat.Builder(this, CHANNEL_MONITOR)
        .setSmallIcon(android.R.drawable.ic_lock_idle_alarm)
        .setContentTitle("HomeGuard")
        .setContentText("Контроль тривоги активний")
        .setOngoing(true)
        .setSilent(true)
        .setContentIntent(
            PendingIntent.getActivity(
                this,
                0,
                Intent(this, MainActivity::class.java),
                PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
            )
        )
        .build()

    private fun startAlarmSignal() {
        if (toneJob?.isActive == true) return
        val vibrator = getSystemService(Context.VIBRATOR_SERVICE) as? Vibrator
        val pattern = longArrayOf(0, 500, 250, 500, 250, 900)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            vibrator?.vibrate(VibrationEffect.createWaveform(pattern, 0))
        } else {
            @Suppress("DEPRECATION")
            vibrator?.vibrate(pattern, 0)
        }
        tone = ToneGenerator(AudioManager.STREAM_ALARM, 100)
        toneJob = scope.launch {
            while (alarmActive) {
                tone?.startTone(ToneGenerator.TONE_CDMA_ALERT_CALL_GUARD, 650)
                delay(900)
            }
        }
    }

    private fun stopAlarmSignal() {
        alarmActive = false
        toneJob?.cancel()
        toneJob = null
        tone?.stopTone()
        tone?.release()
        tone = null
        (getSystemService(Context.VIBRATOR_SERVICE) as? Vibrator)?.cancel()
    }
}
