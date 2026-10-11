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
import ua.homeguard.s3.storage.RegisteredDeviceStore
import ua.homeguard.s3.storage.SettingsStore

class DeviceSession(
    private val scope: CoroutineScope,
    private val endpointProvider: StateFlow<DeviceEndpoint>,
    private val settings: SettingsStore,
    private val telemetry: TelemetrySocket
) {
    …1243 tokens truncated…          if (snapshot.zones.isNotEmpty()) telemetry.acceptFallbackSnapshot(snapshot)
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
        signerSettingsJob?.cancel()
        job?.cancel()
        authorizationJob?.cancel()
        reconnectJob = null
        ticketRefreshJob = null
        bleTelemetryJob = null
        bleStateJob = null
        mqttSettingsJob = null
        signerSettingsJob = null
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

    private fun applyPersistedSigner() {
        val deviceId = settings.settings.value.deviceId
        val url = settings.signerUrl(deviceId)
        if (url.isBlank() || settings.signerToken(deviceId).isBlank()) {
            remoteTransports.clearMqttSigner()
            return
        }
        runCatching {
            remoteTransports.configureMqttSigner(deviceId, url) { settings.signerToken(deviceId) }
        }.onFailure { remoteTransports.clearMqttSigner() }
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
