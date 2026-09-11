package ua.homeguard.s3.network.mqtt

data class MqttConnectionConfig(
    val brokerUri: String = "",
    val username: String = "",
    val deviceId: String = "",
    val enabled: Boolean = false,
) {
    val ready: Boolean
        get() = enabled && brokerUri.isNotBlank() && deviceId.isNotBlank()
}
