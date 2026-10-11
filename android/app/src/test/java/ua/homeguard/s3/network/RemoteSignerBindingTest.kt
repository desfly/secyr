package ua.homeguard.s3.network

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.runBlocking
import org.junit.Assert.*
import org.junit.Test
import ua.homeguard.s3.model.ControlPath
import ua.homeguard.s3.model.DeviceEndpoint
import ua.homeguard.s3.network.mqtt.MqttConnectionConfig
import ua.homeguard.s3.network.mqtt.MqttRuntime

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
}
