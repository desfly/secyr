package ua.homeguard.s3.diagnostics

import ua.homeguard.s3.model.SystemSnapshot

data class DiagnosticItem(
    val label: String,
    val ok: Boolean,
    val detail: String,
)

data class SystemDiagnostics(
    val connectionItems: List<DiagnosticItem>,
    val hardwareItems: List<DiagnosticItem>,
) {
    val connectionReady: Boolean get() = connectionItems.all { it.ok }
    val hardwareTestReady: Boolean get() = hardwareItems.all { it.ok }
}

object SystemDiagnosticsEvaluator {
    fun evaluate(
        deviceId: String,
        route: String,
        localDevices: Int,
        certificateSha256: String,
        snapshot: SystemSnapshot,
        eventCount: Int,
        scanPhase: String = "idle",
        scanNetwork: String = "",
        scanTargets: List<String> = emptyList(),
        scanSent: Int = 0,
        scanReceived: Int = 0,
        scanAccepted: Int = 0,
        scanLastResponder: String = "",
        scanError: String = "",
    ): SystemDiagnostics {
        val targetDetail = if (scanTargets.isEmpty()) "немає" else scanTargets.joinToString(limit = 3, truncated = "…")
        val scanDetail = buildString {
            append(scanPhase)
            if (scanNetwork.isNotBlank()) append(" · ").append(scanNetwork)
            append(" · tx ").append(scanSent)
            append(" / rx ").append(scanReceived)
            append(" / ok ").append(scanAccepted)
            if (scanLastResponder.isNotBlank()) append(" · last ").append(scanLastResponder)
            if (scanError.isNotBlank()) append(" · ").append(scanError)
        }
        val connection = listOf(
            DiagnosticItem("Device ID", deviceId.isNotBlank(), if (deviceId.isBlank()) "не задано" else deviceId),
            DiagnosticItem("Маршрут", route != "OFFLINE", route),
            DiagnosticItem("Локальне виявлення", localDevices > 0 || route == "CLOUD", "знайдено: $localDevices"),
            DiagnosticItem("LAN scan", scanError.isBlank(), scanDetail),
            DiagnosticItem("Broadcast targets", scanTargets.isNotEmpty() || scanPhase == "idle" || route == "CLOUD", targetDetail),
            DiagnosticItem("TLS fingerprint", certificateSha256.isNotBlank() || route == "CLOUD", if (certificateSha256.isBlank()) "не задано" else "налаштовано"),
            DiagnosticItem("Телеметрія", snapshot.sequence > 0, "sequence ${snapshot.sequence}"),
        )
        val hardware = listOf(
            DiagnosticItem("Контролер відповідає", snapshot.sequence > 0, "telemetry sequence ${snapshot.sequence}"),
            DiagnosticItem("Стан системи", snapshot.health.name != "FAILED", snapshot.health.name),
            DiagnosticItem("Зони", snapshot.zones.isNotEmpty(), "каналів: ${snapshot.zones.size}"),
            DiagnosticItem("Аналогові канали", snapshot.pressures.isNotEmpty(), "каналів: ${snapshot.pressures.size}"),
            DiagnosticItem("Журнал подій", eventCount > 0, "подій: $eventCount"),
        )
        return SystemDiagnostics(connection, hardware)
    }
}
