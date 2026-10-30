package ua.homeguard.s3.network

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.runBlocking
import org.junit.Assert.*
import org.junit.Test
import ua.homeguard.s3.model.ControlPath
import ua.homeguard.s3.model.DeviceEndpoint
import ua.homeguard.s3.network.mqtt.MqttConnectionConfig
import ua.homeguard.s3.network.mqtt.MqttRuntime
import ua.homeguard.s3.network.mqtt.MqttRuntimeClient

class RemoteSignerBindingTest {
    @Test fun changingSelectedDeviceCannotUsePreviousSignerCredentials() = runBlocking {
        val runtime = RemoteTransportRuntime(CoroutineScope(Dispatchers.Unconfined),
            MutableStateFlow(DeviceEndpoint("HG-ONE", "", "", ControlPath.OFFLINE)), TelemetrySocket())
        runtime.configureMqtt(MqttConnectionConfig(deviceId = "HG-ONE"))
        runtime.configureMqttSigner("HG-ONE") { _, _, _ -> error("Wrong device reached signer") }
        val mqtt = runtime.javaClass.getDeclaredField("mqtt").apply { isAccessible = true }.get(runtime) as MqttRuntime
        @Suppress("UNCHECKED_CAST")
        val state = mqtt.javaClass.getDeclaredField("state").apply { isAccessible = true }
            .get(mqtt) as MutableStateFlow<MqttRuntime.State>
        state.value = MqttRuntime.State.CONNECTED
        // A wrong device must return before invoking the signer, even while the old MQTT link is healthy.
        assertNull(runtime.executeMqttCommand("HG-TWO", "output.lock"))
        runtime.configureMqtt(MqttConnectionConfig(deviceId = "HG-TWO"))
        assertFalse(runtime.transportStatuses().first { it.kind == TransportKind.MQTT }.commandSupported)
        runtime.stop()
    }

    @Test fun runtimeStartPreservesConfiguredMqttCredential() {
        val testScope = CoroutineScope(Dispatchers.IO + SupervisorJob())
        val runtime = RemoteTransportRuntime(testScope,
            MutableStateFlow(DeviceEndpoint("HG-ONE", "", "", ControlPath.OFFLINE)), TelemetrySocket())
        val settings = MqttConnectionConfig(
            brokerUri = "mqtts://127.0.0.1:1", username = "tester",
            deviceId = "HG-ONE", enabled = true,
        )
        try {
            // Configuration may arrive before the remote runtime is started.
            runtime.configureMqtt(settings, "private-password")
            val client = runtime.javaClass.getDeclaredField("mqttClient")
                .apply { isAccessible = true }.get(runtime) as MqttRuntimeClient
            val workerField = client.javaClass.getDeclaredField("worker")
                .apply { isAccessible = true }
            val configField = client.javaClass.getDeclaredField("config")
                .apply { isAccessible = true }
            val original = workerField.get(client) as Job

            runtime.start()
            assertSame("start() must not reconnect with an empty password",
                original, workerField.get(client))
            assertEquals("private-password",
                (configField.get(client) as MqttRuntimeClient.Config).password)

            runtime.stop()
            runtime.start()
            assertEquals("private-password",
                (configField.get(client) as MqttRuntimeClient.Config).password)
        } finally {
            runtime.stop()
            testScope.cancel()
        }
    }

    @Test fun identicalMqttSettingsPreserveConnectionButPasswordChangeRestarts() {
        val testScope = CoroutineScope(Dispatchers.IO + SupervisorJob())
        val runtime = RemoteTransportRuntime(testScope,
            MutableStateFlow(DeviceEndpoint("HG-ONE", "", "", ControlPath.OFFLINE)), TelemetrySocket())
        // Loopback port 1 deliberately has no broker. The test checks worker
        // lifecycle only and never needs Internet or an external MQTT service.
        val settings = MqttConnectionConfig(
            brokerUri = "mqtts://127.0.0.1:1", username = "tester",
            deviceId = "HG-ONE", enabled = true,
        )
        try {
            runtime.configureMqtt(settings, "first-password")
            val mqttClient = runtime.javaClass.getDeclaredField("mqttClient")
                .apply { isAccessible = true }.get(runtime) as MqttRuntimeClient
            val workerField = mqttClient.javaClass.getDeclaredField("worker")
                .apply { isAccessible = true }
            val original = workerField.get(mqttClient) as Job
            assertTrue(original.isActive)

            // DeviceSession.start() emits the persisted settings twice (direct
            // call and initial StateFlow emission). The second must be a no-op.
            runtime.configureMqtt(settings, "first-password")
            assertSame(original, workerField.get(mqttClient))
            assertTrue(original.isActive)

            // Credentials are part of the effective connection identity.
            runtime.configureMqtt(settings, "rotated-password")
            assertNotSame(original, workerField.get(mqttClient))
            assertFalse(original.isActive)
        } finally {
            runtime.stop()
            testScope.cancel()
        }
    }
}
