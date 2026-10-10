package ua.homeguard.s3.network

import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import ua.homeguard.s3.control.CommandController
import ua.homeguard.s3.storage.EventHistoryStore
import ua.homeguard.s3.storage.RegisteredDeviceStore
import ua.homeguard.s3.storage.SettingsStore

/** One connection owner shared by the screen and the monitoring service. */
class MonitoringRuntime private constructor(context: Context) {
    val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    val settings = SettingsStore(context)
    val registeredDevices = RegisteredDeviceStore(context)
    val eventHistory = EventHistoryStore(context)
    val discovery = LocalDiscoveryCoordinator(context, scope)
    val resolver = DeviceEndpointResolver(settings, discovery, scope)
    val telemetry = TelemetrySocket().apply { seedEvents(eventHistory.load()) }
    val session = DeviceSession(scope, resolver.endpoint, settings, telemetry)
    val commands = CommandController(resolver.endpoint, settings)

    fun start() { discovery.start(); session.start() }
    fun stop() { session.stop(); discovery.stop() }

    companion object {
        @Volatile private var instance: MonitoringRuntime? = null
        fun get(context: Context): MonitoringRuntime = instance ?: synchronized(this) {
            instance ?: MonitoringRuntime(context.applicationContext).also { instance = it }
        }
    }
}
