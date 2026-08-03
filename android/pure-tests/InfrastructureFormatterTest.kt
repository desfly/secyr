import ua.homeguard.s3.model.BatteryMonitorStatus
import ua.homeguard.s3.model.EthernetStatus
import ua.homeguard.s3.model.InfrastructureStatus
import ua.homeguard.s3.model.RtcStatus
import ua.homeguard.s3.model.StorageStatus
import ua.homeguard.s3.ui.infrastructure.InfrastructureFormatter

fun main() {
    val status = InfrastructureStatus(
        rtc = RtcStatus(
            ready = true,
            iso8601 = "2026-08-03T15:00:00+03:00",
            temperatureCelsius = 29.25,
        ),
        ethernet = EthernetStatus(
            initialized = true,
            linkUp = true,
            hasIp = true,
            ipv4 = "192.168.1.50",
        ),
        storage = StorageStatus(
            mounted = true,
            totalBytes = "32000000000",
            freeBytes = "30000000000",
        ),
        battery = BatteryMonitorStatus(
            ready = true,
            voltageV = 12.4,
            currentA = -0.3,
            powerW = -3.72,
        ),
    )

    check(
        InfrastructureFormatter.ethernet(status)
            .contains("192.168.1.50")
    )
    check(
        InfrastructureFormatter.rtc(status)
            .contains("2026-08-03")
    )
    check(
        InfrastructureFormatter.storage(status)
            .contains("30000000000")
    )
    check(
        InfrastructureFormatter.battery(status)
            .contains("12.40 V")
    )

    println("Infrastructure formatter tests PASS")
}
