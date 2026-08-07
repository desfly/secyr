package ua.homeguard.s3.notifications

import android.Manifest
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import androidx.core.content.ContextCompat
import ua.homeguard.s3.MainActivity
import ua.homeguard.s3.model.SystemEventRecord

class HomeGuardNotifications(private val context: Context) {
    companion object {
        const val CHANNEL_CRITICAL = "homeguard_critical"
        const val CHANNEL_STATUS = "homeguard_status"
    }

    fun createChannels() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return
        val manager = context.getSystemService(NotificationManager::class.java)
        manager.createNotificationChannel(
            NotificationChannel(
                CHANNEL_CRITICAL,
                "HomeGuard critical alerts",
                NotificationManager.IMPORTANCE_HIGH,
            ).apply { description = "Alarm, tamper and critical HomeGuard alerts" }
        )
        manager.createNotificationChannel(
            NotificationChannel(
                CHANNEL_STATUS,
                "HomeGuard status",
                NotificationManager.IMPORTANCE_DEFAULT,
            ).apply { description = "Arming, zones and device status" }
        )
    }

    fun notify(event: SystemEventRecord) {
        val alert = AlertPolicy.classify(event) ?: return
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
            ContextCompat.checkSelfPermission(context, Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED
        ) return

        val channel = if (alert.severity == AlertSeverity.CRITICAL) CHANNEL_CRITICAL else CHANNEL_STATUS
        val intent = Intent(context, MainActivity::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_SINGLE_TOP
        }
        val pendingIntent = PendingIntent.getActivity(
            context,
            0,
            intent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
        )
        val notification = NotificationCompat.Builder(context, channel)
            .setSmallIcon(android.R.drawable.ic_dialog_alert)
            .setContentTitle(alert.title)
            .setContentText(alert.text)
            .setStyle(NotificationCompat.BigTextStyle().bigText(alert.text))
            .setPriority(if (alert.severity == AlertSeverity.CRITICAL) NotificationCompat.PRIORITY_MAX else NotificationCompat.PRIORITY_DEFAULT)
            .setCategory(if (alert.severity == AlertSeverity.CRITICAL) NotificationCompat.CATEGORY_ALARM else NotificationCompat.CATEGORY_STATUS)
            .setAutoCancel(true)
            .setContentIntent(pendingIntent)
            .build()

        NotificationManagerCompat.from(context).notify(notificationId(event), notification)
    }

    private fun notificationId(event: SystemEventRecord): Int {
        val sequence = event.sequence xor (event.sequence ushr 32)
        return (sequence.toInt() and 0x7fffffff).coerceAtLeast(1)
    }
}
