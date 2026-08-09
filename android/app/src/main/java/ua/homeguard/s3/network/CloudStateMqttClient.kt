package ua.homeguard.s3.network

import com.hivemq.client.mqtt.MqttClient
import com.hivemq.client.mqtt.datatypes.MqttQos
import com.hivemq.client.mqtt.mqtt3.Mqtt3AsyncClient
import java.nio.charset.StandardCharsets
import java.util.UUID
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow

/**
 * Build-0060 cloud telemetry bridge.
 *
 * The public Mosquitto endpoint is TEST-ONLY. Production must replace this
 * with the authenticated HomeGuard broker/service endpoint.
 */
class CloudStateMqttClient(
    private val telemetry: TelemetrySocket,
) {
    companion object {
        private const val TEST_BROKER_HOST = "test.mosquitto.org"
        private const val TEST_BROKER_PORT = 8886
    }

    private var client: Mqtt3AsyncClient? = null
    private var activeDeviceId: String = ""
    private val mutableStatus = MutableStateFlow("cloud idle")
    val status: StateFlow<String> = mutableStatus

    @Synchronized
    fun connect(deviceId: String) {
        val normalized = deviceId.trim()
        if (normalized.isBlank()) {
            disconnect()
            return
        }
        if (activeDeviceId == normalized && client != null) return

        disconnect()
        activeDeviceId = normalized
        mutableStatus.value = "cloud connecting"

        val mqtt = MqttClient.builder()
            .identifier("hg-android-${UUID.randomUUID()}")
            .serverHost(TEST_BROKER_HOST)
            .serverPort(TEST_BROKER_PORT)
            .sslWithDefaultConfig()
            .useMqttVersion3()
            .buildAsync()
        client = mqtt

        mqtt.connect().whenComplete { _, connectError ->
            if (client !== mqtt) return@whenComplete
            if (connectError != null) {
                mutableStatus.value = "cloud error: ${connectError.message ?: "connect"}"
                return@whenComplete
            }

            val topic = "homeguard/v1/devices/$normalized/state"
            mqtt.subscribeWith()
                .topicFilter(topic)
                .qos(MqttQos.AT_LEAST_ONCE)
                .callback { publish ->
                    if (client !== mqtt) return@callback
                    val payload = String(publish.payloadAsBytes, StandardCharsets.UTF_8)
                    if (telemetry.acceptCloudState(payload)) {
                        mutableStatus.value = "cloud connected"
                    }
                }
                .send()
                .whenComplete { _, subscribeError ->
                    if (client !== mqtt) return@whenComplete
                    if (subscribeError != null) {
                        mutableStatus.value = "cloud error: ${subscribeError.message ?: "subscribe"}"
                    } else {
                        mutableStatus.value = "cloud subscribed"
                    }
                }
        }
    }

    @Synchronized
    fun disconnect() {
        val previous = client
        client = null
        activeDeviceId = ""
        if (previous != null) {
            runCatching { previous.disconnect() }
        }
        mutableStatus.value = "cloud idle"
    }
}
