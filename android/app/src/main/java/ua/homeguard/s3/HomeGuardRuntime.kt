package ua.homeguard.s3

import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import com.google.firebase.messaging.FirebaseMessaging
import ua.homeguard.s3.push.PushTokenRegistrar
import ua.homeguard.s3.auth.CloudAccountAuth
import ua.homeguard.s3.network.DeviceEndpointResolver
import ua.homeguard.s3.network.DeviceSession
import ua.homeguard.s3.network.LocalDiscoveryCoordinator
import ua.homeguard.s3.network.TelemetrySocket
import ua.homeguard.s3.storage.SettingsStore

/**
 * Process-wide controller runtime. It is deliberately not owned by an Activity:
 * leaving the UI must not tear down HomeGuard telemetry/alarm monitoring.
 */
object HomeGuardRuntime {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)

    lateinit var settings: SettingsStore
        private set
    lateinit var discovery: LocalDiscoveryCoordinator
        private set
    lateinit var resolver: DeviceEndpointResolver
        private set
    lateinit var telemetry: TelemetrySocket
        private set
    lateinit var session: DeviceSession
        private set

    @Synchronized
    fun ensureStarted(context: Context) {
        if (::session.isInitialized) return
        val app = context.applicationContext
        settings = SettingsStore(app)
        discovery = LocalDiscoveryCoordinator(app, scope)
        resolver = DeviceEndpointResolver(settings, discovery, scope)
        telemetry = TelemetrySocket()
        session = DeviceSession(scope, resolver.endpoint, settings, telemetry)
        discovery.start()
        session.start()
        scope.launch {
            settings.settings
                .map { Pair(it.deviceId.trim(), it.cloudBaseUrl.trim()) }
                .distinctUntilChanged()
                .collect { (deviceId, cloudBaseUrl) ->
                    if (deviceId.isBlank() || cloudBaseUrl.isBlank() || !CloudAccountAuth.signedIn()) return@collect
                    FirebaseMessaging.getInstance().token
                        .addOnSuccessListener { token ->
                            if (token.isBlank()) return@addOnSuccessListener
                            scope.launch {
                                runCatching { PushTokenRegistrar.register(settings.settings.value, token) }
                            }
                        }
                }
        }
    }
}
