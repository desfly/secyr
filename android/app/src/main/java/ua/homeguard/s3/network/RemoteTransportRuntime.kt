package ua.homeguard.s3.network

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.combine
import ua.homeguard.s3.network.cloud.CloudRuntime
import ua.homeguard.s3.network.mqtt.MqttConnectionConfig
import ua.homeguard.s3.network.mqtt.MqttRuntime
import ua.homeguard.s3.network.mqtt.MqttRuntimeClient
import ua.homeguard.s3.model.SystemEventRecord

/**
 * Coordinates remote transports without coupling them.
 *
 * CLOUD HTTP/WSS and MQTT have separate lifecycles. This class only exposes
 * their health to the generic transport selector; neither transport uses the
 * other as an implementation detail.
 */
class RemoteTransportRuntime(
    private val scope: CoroutineScope,
    endpointProvider: StateFlow<ua.homeguard.s3.model.DeviceEndpoint>,
    private val telemetry: TelemetrySocket,
) {
    private val cloud = CloudRuntime(scope, endpointProvider, telemetry)
    private val mqttClient = MqttRuntimeClient(scope)
    private val mqtt = MqttRuntime(scope, mqttClient)
    private var mqttEventsJob: Job? = null
    private var mqttStateJob: Job? = null

    private val mqttConfig = MutableStateFlow(MqttConnectionConfig())

    fun cloudState(): StateFlow<CloudRuntime.State> = cloud.state()
    fun cloudLastSeenAtMs(): StateFlow<Long> = cloud.lastSeenAtMs()
    fun mqttState(): StateFlow<MqttRuntime.State> = mqtt.state()
    fun mqttLastSeenAtMs(): StateFlow<Long> = mqtt.lastSeenAtMs()
    fun mqttConfiguration(): StateFlow<MqttConnectionConfig> = mqttConfig.asStateFlow()

    fun start() {
        cloud.start()
        if (mqttEventsJob == null) {
            mqttEventsJob = scope.launch {
                mqttClient.events().collect { json ->
                    val eventName = json.optString("event", "unknown")
                    val source = if (json.has("sourceId")) json.optInt("sourceId", 0) else json.optInt("source", 0)
                    val timestamp = if (json.has("timestampMs")) json.optLong("timestampMs", 0L) else json.optLong("ts", 0L)
                    val sequence = if (json.has("sequence")) json.optLong("sequence", 0L) else json.optLong("seq", 0L)
                    telemetry.acceptExternalEvent(
                        SystemEventRecord(
                            sequence = sequence,
                            timestampMs = timestamp,
                            event = eventName,
                            sourceId = source,
                            value = json.optInt("value", 0),
                        ),
                    )
                }
            }
        }
        if (mqttStateJob == null) {
            mqttStateJob = scope.launch {
                combine(mqttClient.deviceState(), mqtt.state()) { device, health ->
                    if (health == MqttRuntime.State.CONNECTED) device?.mode else null
                }.collect { telemetry.acceptMqttMode(it) }
            }
        }
        applyMqttConfig(mqttConfig.value)
    }

    fun stop() {
        cloud.stop()
        mqtt.stop()
        mqttEventsJob?.cancel()
        mqttEventsJob = null
        mqttStateJob?.cancel()
        mqttStateJob = null
        telemetry.acceptMqttMode(null)
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
                commandSupported = false, // Enable only after server signing and command routing are configured.
            ),
        )
    }

    fun chooseRemoteCommand(): TransportKind? =
        TransportMatrix.chooseCommand(transportStatuses())

    fun chooseRemoteTelemetry(): TransportKind? =
        TransportMatrix.chooseTelemetry(transportStatuses())

    private fun applyMqttConfig(config: MqttConnectionConfig, credential: String = "") {
        mqtt.stop()
        telemetry.acceptMqttMode(null)
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
