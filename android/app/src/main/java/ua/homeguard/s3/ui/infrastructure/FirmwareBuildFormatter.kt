package ua.homeguard.s3.ui.infrastructure

import ua.homeguard.s3.model.FirmwareBuildStatus

object FirmwareBuildFormatter {
    fun summary(status: FirmwareBuildStatus): String =
        if (status.compiled) {
            "Firmware ${status.firmwareVersion}, " +
                "Build-${status.buildNumber}, ESP-IDF ${status.espIdfVersion}"
        } else {
            "Build-${status.buildNumber}: firmware binary not confirmed"
        }

    fun ota(status: FirmwareBuildStatus): String =
        if (status.otaCapable) {
            "OTA: factory + ota_0 + ota_1"
        } else {
            "OTA: unavailable"
        }
}
