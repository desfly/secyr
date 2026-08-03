package ua.homeguard.s3.ui.infrastructure

import java.util.Locale
import ua.homeguard.s3.model.InfrastructureStatus

object InfrastructureFormatter {
    fun ethernet(status: InfrastructureStatus): String =
        when {
            !status.ethernet.initialized ->
                "LAN: не ініціалізовано"
            !status.ethernet.linkUp ->
                "LAN: кабель не підключено"
            !status.ethernet.hasIp ->
                "LAN: очікування IP"
            else ->
                "LAN: ${status.ethernet.ipv4}"
        }

    fun rtc(status: InfrastructureStatus): String =
        if (status.rtc.ready) {
            "RTC: ${status.rtc.iso8601}"
        } else {
            "RTC: несправність"
        }

    fun storage(status: InfrastructureStatus): String =
        if (!status.storage.mounted) {
            "microSD: не змонтована"
        } else {
            "microSD: ${status.storage.freeBytes} байт вільно"
        }

    fun battery(status: InfrastructureStatus): String =
        if (!status.battery.ready) {
            "Акумулятор: INA226 недоступний"
        } else {
            String.format(
                Locale.US,
                "Акумулятор: %.2f V, %.2f A, %.2f W",
                status.battery.voltageV,
                status.battery.currentA,
                status.battery.powerW,
            )
        }
}
