package ua.homeguard.s3.model

data class OneWireRuntimeStatus(
    val ready: Boolean,
    val discoveredSensors: Int,
    val validSensors: Int,
)

data class Rs485RuntimeStatus(
    val ready: Boolean,
    val baudRate: Int,
    val lastResponseOk: Boolean,
)

data class RuntimeIntegrationStatus(
    val oneWire: OneWireRuntimeStatus,
    val rs485: Rs485RuntimeStatus,
    val telemetryTaskRunning: Boolean,
    val hardwareEndpointAvailable: Boolean,
)
