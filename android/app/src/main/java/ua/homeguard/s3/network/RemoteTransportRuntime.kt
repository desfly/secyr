package ua.homeguard.s3.network

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import ua.homeguard.s3.network.cloud.CloudRuntime
import ua.homeguard.s3.network.mqtt.MqttConnectionConfig
import ua.homeguard.s3.network.mqtt.MqttRuntime
import ua.homeguard.s3.network.mqtt.MqttRuntimeClient

/**
 * Coordinates remote transports without coupling them.
 *
 * CLOUD HTTP/WSS and MQTT have separate lifecycles. This class only exposes
 * their health to the generic transport selector; neither transport uses the
 * other as an implementation detail.
 */
class RemoteTransportRuntime(
    scope: CoroutineScope,
    endpointProvider: StateFlow<ua.homeguard.s3.model.DeviceEndpoint>,
    telemetry: TelemetrySocket,
) {
    private val cloud = CloudRuntime(scope, endpointProvider, telemetry)
    private val mqttClient = MqttRuntimeClient(scope)
    private val mqtt = MqttRuntime(scope, mqttClient)

    private val mqttConfig = MutableStateFlow(MqttConnectionConfig())

    fun cloudState(): StateFlow<CloudRuntime.State> = cloud.state()
    fun cloudLastSeenAtMs(): StateFlow<Long> = cloud.lastSeenAtMs()
    fun mqttState(): StateFlow<MqttRuntime.State> = mqtt.state()
    fun mqttLastSeenAtMs(): StateFlow<Long> = mqtt.lastSeenAtMs()
    fun mqttConfiguration(): StateFlow<MqttConnectionConfig> = mqttConfig.asStateFlow()

    fun start() {
        cloud.start()
        applyMqttConfig(mqttConfig.value)
    }

    fun stop() {
        cloud.stop()
        mqtt.stop()
    }

    fun configureMqtt(config: MqttConnectionConfig, credential: String = "") {
        mqttConfig.value = config
        applyMqttConfig(config, credential)
    }

    fun transportStatuses(): List<TransportStatus> {
        val cloudState = cloud.state().value
        val mqttState = mqtt.state().value
        val cloudSeen = cloud.lastSeenAtMs().value
        val mqttSeen = mqtt.lastSeenAtMs().value
        return listOf(
            TransportStatus(
                kind = TransportKind.CLOUD_HTTP,
                available = cloudState == CloudRuntime.State.CONNECTED,
                authenticated = cloudState == CloudRuntime.State.CONNECTED,
                lastSeenAtMs = cloudSeen,
            ),
            TransportStatus(
                kind = TransportKind.CLOUD_WSS,
                available = cloudState == CloudRuntime.State.CONNECTED,
                authenticated = cloudState == CloudRuntime.State.CONNECTED,
                lastSeenAtMs = cloudSeen,
            ),
            TransportStatus(
                kind = TransportKind.MQTT,
                available = mqttState == MqttRuntime.State.CONNECTED,
                authenticated = mqttState == MqttRuntime.State.CONNECTED,
                lastSeenAtMs = mqttSeen,
            ),
        )
    }

    fun chooseRemoteCommand(): TransportKind? =
        TransportMatrix.chooseCommand(transportStatuses())

    fun chooseRemoteTelemetry(): TransportKind? =
        TransportMatrix.chooseTelemetry(transportStatuses())

    private fun applyMqttConfig(config: MqttConnectionConfig, credential: String = "") {
        mqtt.stop()
        if (!config.ready) return
        mqtt.start(
            MqttRuntimeClient.Config(
                brokerUri = config.brokerUri,
                username = config.username,
                password = credential,
                deviceId = config.deviceId,
            ),
        )
    }
}
