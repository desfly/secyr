package ua.homeguard.s3.notifications

import android.Manifest
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.media.AudioAttributes
import android.media.RingtoneManager
import android.content.pm.PackageManager
import android.os.Build
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import androidx.core.content.ContextCompat
import ua.homeguard.s3.MainActivity
import ua.homeguard.s3.model.SystemEventRecord
import ua.homeguard.s3.storage.AppSettings

class HomeGuardNotifications(private val context: Context) {
    companion object {
        const val CHANNEL_CRITICAL = "homeguard_critical_alarm_v2"
        const val CHANNEL_STATUS = "homeguard_status"
    }

    fun createChannels() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return
        val manager = context.getSystemService(NotificationManager::class.java)
        val alarmSound = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_ALARM)
            ?: RingtoneManager.getDefaultUri(RingtoneManager.TYPE_NOTIFICATION)
        manager.createNotificationChannel(
            NotificationChannel(
                CHANNEL_CRITICAL,
                "HomeGuard · ТРИВОГА",
                NotificationManager.IMPORTANCE_HIGH,
            ).apply {
                description = "Тривога HomeGuard: звук і вібрація"
                enableVibration(true)
                vibrationPattern = longArrayOf(0, 500, 250, 500, 250, 900)
                setSound(
                    alarmSound,
                    AudioAttributes.Builder()
                        .setUsage(AudioAttributes.USAGE_ALARM)
                        .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                        .build(),
                )
                lockscreenVisibility = android.app.Notification.VISIBILITY_PUBLIC
            }
        )
        manager.createNotificationChannel(
            NotificationChannel(
                CHANNEL_STATUS,
                "HomeGuard status",
                NotificationManager.IMPORTANCE_DEFAULT,
            ).apply { description = "Arming, zones and device status" }
        )
    }

    fun notify(event: SystemEventRecord, settings: AppSettings) {
        val alert = AlertPolicy.classify(event) ?: return
        if (!isEnabled(event, alert, settings)) return
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
            ContextCompat.checkSelfPermission(context, Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED
        ) return

        val critical = alert.severity != AlertSeverity.INFO
        val channel = if (critical) CHANNEL_CRITICAL else CHANNEL_STATUS
        val intent = Intent(context, MainActivity::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_SINGLE_TOP
        }
        val pendingIntent = PendingIntent.getActivity(
            context,
            0,
            intent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
        )
        val builder = NotificationCompat.Builder(context, channel)
            .setSmallIcon(android.R.drawable.ic_dialog_alert)
            .setContentTitle(alert.title)
            .setContentText(alert.text)
            .setStyle(NotificationCompat.BigTextStyle().bigText(alert.text))
            .setPriority(if (critical) NotificationCompat.PRIORITY_MAX else NotificationCompat.PRIORITY_DEFAULT)
            .setCategory(if (critical) NotificationCompat.CATEGORY_ALARM else NotificationCompat.CATEGORY_STATUS)
            .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
            .setAutoCancel(true)
            .setContentIntent(pendingIntent)

        if (critical && Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            val alarmSound = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_ALARM)
                ?: RingtoneManager.getDefaultUri(RingtoneManager.TYPE_NOTIFICATION)
            builder
                .setSound(alarmSound)
                .setVibrate(longArrayOf(0, 500, 250, 500, 250, 900))
        }
        val notification = builder.build()

        NotificationManagerCompat.from(context).notify(notificationId(event), notification)
    }

    private fun isEnabled(event: SystemEventRecord, alert: AlertMessage, settings: AppSettings): Boolean {
        val type = event.event.uppercase()
        if (type == "ZONE_OPEN" || type == "ZONE_CLOSED") {
            return settings.statusNotificationsEnabled && settings.zoneNotificationsEnabled
        }
        return if (alert.severity == AlertSeverity.INFO) {
            settings.statusNotificationsEnabled
        } else {
            settings.criticalNotificationsEnabled
        }
    }

    private fun notificationId(event: SystemEventRecord): Int {
        val sequence = event.sequence xor (event.sequence ushr 32)
        return (sequence.toInt() and 0x7fffffff).coerceAtLeast(1)
    }
}
