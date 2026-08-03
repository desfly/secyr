package ua.homeguard.s3.ui.infrastructure

import ua.homeguard.s3.model.RuntimeIntegrationStatus

object RuntimeIntegrationFormatter {
    fun oneWire(status: RuntimeIntegrationStatus): String =
        if (!status.oneWire.ready) {
            "1-Wire: несправність"
        } else {
            "1-Wire: ${status.oneWire.validSensors}/" +
                "${status.oneWire.discoveredSensors} датчиків"
        }

    fun rs485(status: RuntimeIntegrationStatus): String =
        if (!status.rs485.ready) {
            "RS-485: не готовий"
        } else {
            "RS-485: ${status.rs485.baudRate} бод, " +
                if (status.rs485.lastResponseOk) "зв'язок OK" else "немає відповіді"
        }
}
