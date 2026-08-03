package ua.homeguard.s3.ui.dashboard

import java.util.Locale
import ua.homeguard.s3.model.PressureSensorStatus
import ua.homeguard.s3.model.SupervisedZoneStatus
import ua.homeguard.s3.model.TemperatureChannelStatus
import ua.homeguard.s3.model.WaterValveStatus

object HardwareDashboardFormatter {
    fun zone(status: SupervisedZoneStatus): String =
        "${status.title}: ${status.state.name}, " +
            String.format(
                Locale.US,
                "%.0f mV",
                status.filteredMillivolts,
            )

    fun pressure(status: PressureSensorStatus): String =
        "${status.title}: " +
            String.format(
                Locale.US,
                "%.2f bar / %.2f mA",
                status.filteredBar,
                status.currentMilliamp,
            ) +
            " (${status.state.name})"

    fun valve(status: WaterValveStatus): String =
        "${status.title}: ${status.state.name}" +
            if (status.emergencyLatched) " [EMERGENCY]" else ""

    fun temperature(status: TemperatureChannelStatus): String =
        if (!status.valid) {
            "${status.title}: SENSOR FAULT"
        } else {
            "${status.title}: " +
                String.format(
                    Locale.US,
                    "%.1f °C, avg %.1f °C, Δ %.2f °C/min",
                    status.latestCelsius,
                    status.averageCelsius,
                    status.rateCelsiusPerMinute,
                )
        }
}
