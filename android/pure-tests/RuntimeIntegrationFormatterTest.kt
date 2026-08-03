import ua.homeguard.s3.model.OneWireRuntimeStatus
import ua.homeguard.s3.model.Rs485RuntimeStatus
import ua.homeguard.s3.model.RuntimeIntegrationStatus
import ua.homeguard.s3.ui.infrastructure.RuntimeIntegrationFormatter

fun main() {
    val status = RuntimeIntegrationStatus(
        oneWire = OneWireRuntimeStatus(
            ready = true,
            discoveredSensors = 2,
            validSensors = 2,
        ),
        rs485 = Rs485RuntimeStatus(
            ready = true,
            baudRate = 9600,
            lastResponseOk = true,
        ),
        telemetryTaskRunning = true,
        hardwareEndpointAvailable = true,
    )

    check(
        RuntimeIntegrationFormatter.oneWire(status)
            .contains("2/2")
    )
    check(
        RuntimeIntegrationFormatter.rs485(status)
            .contains("9600")
    )
    check(
        RuntimeIntegrationFormatter.rs485(status)
            .contains("OK")
    )

    println("Runtime integration formatter tests PASS")
}
