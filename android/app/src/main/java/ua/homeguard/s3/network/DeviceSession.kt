package ua.homeguard.s3.network

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import ua.homeguard.s3.model.ControlPath
import ua.homeguard.s3.model.DeviceEndpoint
import ua.homeguard.s3.network.ble.BleHomeGuardClient
import ua.homeguard.s3.network.ble.BleRuntimeRegistry
import ua.homeguard.s3.network.cloud.CloudRuntime
import ua.homeguard.s3.network.mqtt.MqttConnectionConfig
import ua.homeguard.s3.network.mqtt.MqttRuntime
import ua.homeguard.s3.network.mqtt.MqttCommandEnvelope
import ua.homeguard.s3.network.mqtt.MqttCommandSigner
import org.json.JSONObject
import ua.homeguard.s3.storage.RegisteredDeviceStore
import ua.homeguard.s3.storage.SettingsStore

class DeviceSession(
    private val scope: CoroutineScope,
    private val endpointProvider: StateFlow<DeviceEndpoint>,
    private val settings: SettingsStore,
    private val telemetry: TelemetrySocket
) {
    companion object {
        private const val RECONNECT_DELAY_MS = 2_000L
    }

    private val remoteTransports = RemoteTransportRuntime(scope, endpointProvider, telemetry)
    private val ble = BleRuntimeRegistry.get(settings.appContext)
    private var job: Job? = null
    private var authorizationJob: Job? = null
    private var reconnectJob: Job? = null
    private var ticketRefreshJob: Job? = null
    private var bleTelemetryJob: Job? = null
    private var bleStateJob: Job? = null
    private var mqttSettingsJob: Job? = null
    @Volatile private var activeTarget: SessionTarget? = null

    fun cloudState(): StateFlow<CloudRuntime.State> = remoteTransports.cloudState()
    fun mqttState(): StateFlow<MqttRuntime.State> = remoteTransports.mqttState()
    fun remoteStatuses(): List<TransportStatus> = remoteTransports.transportStatuses()
    fun configureMqtt(config: MqttConnectionConfig, credential: String = "") =
        remoteTransports.configureMqtt(config, credential)

    suspend fun mqttCommand(
        envelope: MqttCommandEnvelope,
        signer: MqttCommandSigner,
        timeoutMs: Long = 8_000L,
    ): JSONObject = remoteTransports.mqttCommand(envelope, signer, timeoutMs)

    fun start() {
        if (job != null) return
        remoteTransports.start()
        applyPersistedMqtt()
        mqttSettingsJob = scope.launch {
            settings.settings
                .map { Triple(it.mqttBrokerUri, it.mqttUsername, it.deviceId) }
                .distinctUntilChanged()
                .collect { applyPersistedMqtt() }
        }
        job = scope.launch {
            combine(endpointProvider, settings.settings) { endpoint, appSettings ->
                when (endpoint.path) {
                    ControlPath.CLOUD -> SessionTarget(endpoint, appSettings.apiToken, false)
                    ControlPath.OFFLINE -> SessionTarget(endpoint, "", false)
                    else -> SessionTarget(
                        endpoint = endpoint,
                        token = appSettings.telemetryToken.ifBlank { appSettings.apiToken },
                        oneShotTicket = appSettings.telemetryToken.isNotBlank(),
                    )
                }
            }.distinctUntilChanged().collect { target: SessionTarget ->
                val previous = activeTarget
                activeTarget = target
                reconnectJob?.cancel()
                reconnectJob = null

                // A telemetry ticket is consumed by the WebSocket upgrade. If
                // local routing fails over between Wi-Fi and W5500, never reuse
                // the ticket that authenticated the old socket. Mint a fresh
                // ticket on the newly selected route before reconnecting.
                val localRouteChanged =
                    previous != null &&
                        isLocal(previous.endpoint) &&
                        isLocal(target.endpoint) &&
                        previous.endpoint.apiBaseUrl != target.endpoint.apiBaseUrl

                if (localRouteChanged && target.oneShotTicket) {
                    refreshLocalTicket(settings.settings.value.deviceId, allowDurableFallback = false)
                } else {
                    connectTarget(target)
                }
            }
        }

        authorizationJob = scope.launch {
            telemetry.connection().collect { state ->
                val deviceId = settings.settings.value.deviceId
                when (state) {
                    TelemetryConnectionState.UNAUTHORIZED -> handleUnauthorized(deviceId)
                    TelemetryConnectionState.CONNECTED -> {
                        reconnectJob?.cancel()
                        reconnectJob = null
                        ticketRefreshJob?.cancel()
                        ticketRefreshJob = null
                        RegisteredDeviceStore.markActiveAuthorization(deviceId, true)
                    }
                    TelemetryConnectionState.OFFLINE -> scheduleReconnect()
                    else -> Unit
                }
            }
        }

        bleTelemetryJob = scope.launch {
            ble.snapshots().collect { snapshot ->
                if (ble.isReady()) telemetry.acceptFallbackSnapshot(snapshot)
            }
        }

        bleStateJob = scope.launch {
            ble.state().collect { state ->
                if (state == BleHomeGuardClient.State.READY) {
                    val snapshot = ble.snapshots().value
                    if (snapshot.zones.isNotEmpty()) telemetry.acceptFallbackSnapshot(snapshot)
                } else {
                    telemetry.clearFallbackSnapshot()
                }
            }
        }
    }

    fun stop() {
        activeTarget = null
        reconnectJob?.cancel()
        ticketRefreshJob?.cancel()
        bleTelemetryJob?.cancel()
        bleStateJob?.cancel()
        mqttSettingsJob?.cancel()
        job?.cancel()
        authorizationJob?.cancel()
        reconnectJob = null
        ticketRefreshJob = null
        bleTelemetryJob = null
        bleStateJob = null
        mqttSettingsJob = null
        job = null
        authorizationJob = null
        remoteTransports.stop()
        telemetry.clearFallbackSnapshot()
        telemetry.disconnect()
    }

    private fun applyPersistedMqtt() {
        val appSettings = settings.settings.value
        val deviceId = appSettings.deviceId
        remoteTransports.configureMqtt(
            MqttConnectionConfig(
                brokerUri = appSettings.mqttBrokerUri,
                username = appSettings.mqttUsername,
                deviceId = deviceId,
                enabled = appSettings.mqttBrokerUri.isNotBlank() && deviceId.isNotBlank(),
            ),
            settings.mqttPassword(),
        )
    }

    private fun connectTarget(target: SessionTarget) {
        val endpoint = target.endpoint
        if (endpoint.path == ControlPath.OFFLINE || endpoint.websocketUrl.isBlank() || target.token.isBlank()) {
            telemetry.disconnect()
            return
        }
        val pin = if (endpoint.path == ControlPath.CLOUD) "" else endpoint.certificateSha256
        telemetry.connect(endpoint.websocketUrl, target.token, pin)
    }

    private suspend fun handleUnauthorized(deviceId: String) {
        reconnectJob?.cancel()
        reconnectJob = null
        val target = activeTarget ?: return
        if (isLocal(target.endpoint) && target.oneShotTicket) {
            // A rejected/expired one-shot telemetry ticket is not evidence that
            // the user's saved login was revoked. Refresh it through the
            // authenticated HTTP session and keep authorization intact.
            refreshLocalTicket(deviceId, allowDurableFallback = false)
            return
        }
        if (!isLocal(target.endpoint)) {
            RegisteredDeviceStore.markActiveAuthorization(deviceId, false)
        }
    }

    private fun scheduleReconnect() {
        if (!settings.settings.value.autoReconnect) return
        if (reconnectJob?.isActive == true || ticketRefreshJob?.isActive == true) return
        val target = activeTarget ?: return
        if (target.endpoint.path == ControlPath.OFFLINE || target.endpoint.websocketUrl.isBlank() || target.token.isBlank()) return

        reconnectJob = scope.launch {
            while (activeTarget == target && telemetry.connection().value == TelemetryConnectionState.OFFLINE) {
                delay(RECONNECT_DELAY_MS)
                if (activeTarget != target || telemetry.connection().value != TelemetryConnectionState.OFFLINE) return@launch

                if (isLocal(target.endpoint) && target.oneShotTicket) {
                    val refreshed = runCatching { LocalTelemetryTicketBroker.refresh() }
                    if (refreshed.isSuccess) return@launch
                    // No authenticated HTTP session yet usually means app
                    // startup is still restoring the saved login. Do not
                    // mislabel the controller as revoked; the login path will
                    // issue a fresh ticket and update settings.
                    continue
                }

                connectTarget(target)
                return@launch
            }
        }
    }

    private fun refreshLocalTicket(deviceId: String, allowDurableFallback: Boolean) {
        if (ticketRefreshJob?.isActive == true) return
        ticketRefreshJob = scope.launch {
            val result = runCatching { LocalTelemetryTicketBroker.refresh() }
            if (result.isSuccess) return@launch

            val current = settings.settings.value
            if (allowDurableFallback && current.telemetryToken.isNotBlank() && current.apiToken.isNotBlank()) {
                settings.update(current.copy(telemetryToken = ""))
            }
            // A telemetry refresh failure is transport/session state, not proof
            // that the saved account was revoked. Authorization is changed only
            // by an explicit login/command 401 path.
            if (deviceId.isBlank()) telemetry.disconnect()
        }
    }

    private fun isLocal(endpoint: DeviceEndpoint): Boolean =
        endpoint.path != ControlPath.CLOUD && endpoint.path != ControlPath.OFFLINE

    private data class SessionTarget(
        val endpoint: DeviceEndpoint,
        val token: String,
        val oneShotTicket: Boolean,
    )
}
