package ua.homeguard.s3.model

data class RtcStatus(
    val ready: Boolean,
    val iso8601: String,
    val temperatureCelsius: Double,
)

data class EthernetStatus(
    val initialized: Boolean,
    val linkUp: Boolean,
    val hasIp: Boolean,
    val ipv4: String,
)

data class StorageStatus(
    val mounted: Boolean,
    val totalBytes: String,
    val freeBytes: String,
)

data class BatteryMonitorStatus(
    val ready: Boolean,
    val voltageV: Double,
    val currentA: Double,
    val powerW: Double,
)

data class InfrastructureStatus(
    val rtc: RtcStatus,
    val ethernet: EthernetStatus,
    val storage: StorageStatus,
    val battery: BatteryMonitorStatus,
)
