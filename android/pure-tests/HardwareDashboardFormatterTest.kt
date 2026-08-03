import ua.homeguard.s3.model.PressureSensorState
import ua.homeguard.s3.model.PressureSensorStatus
import ua.homeguard.s3.model.SupervisedZoneState
import ua.homeguard.s3.model.SupervisedZoneStatus
import ua.homeguard.s3.model.TemperatureChannelStatus
import ua.homeguard.s3.model.WaterValveState
import ua.homeguard.s3.model.WaterValveStatus
import ua.homeguard.s3.ui.dashboard.HardwareDashboardFormatter

fun main() {
    val zone = HardwareDashboardFormatter.zone(
        SupervisedZoneStatus(
            id = "motion",
            title = "Рух",
            rawMillivolts = 1502.0,
            filteredMillivolts = 1500.0,
            state = SupervisedZoneState.NORMAL,
            transitionCount = 2,
        )
    )
    check(zone.contains("1500 mV"))
    check(zone.contains("NORMAL"))

    val pressure = HardwareDashboardFormatter.pressure(
        PressureSensorStatus(
            title = "Холодна вода",
            pressureBar = 5.0,
            filteredBar = 4.98,
            currentMilliamp = 12.0,
            rateBarPerSecond = 0.0,
            state = PressureSensorState.OK,
        )
    )
    check(pressure.contains("4.98 bar"))
    check(pressure.contains("12.00 mA"))

    val valve = HardwareDashboardFormatter.valve(
        WaterValveStatus(
            title = "Гаряча вода",
            state = WaterValveState.CLOSED,
            faultCount = 0,
            emergencyLatched = true,
        )
    )
    check(valve.contains("EMERGENCY"))

    val temperature = HardwareDashboardFormatter.temperature(
        TemperatureChannelStatus(
            title = "Гаряча вода",
            valid = true,
            latestCelsius = 48.2,
            averageCelsius = 47.9,
            minimumCelsius = 47.0,
            maximumCelsius = 49.0,
            rateCelsiusPerMinute = 0.12,
        )
    )
    check(temperature.contains("48.2"))
    check(temperature.contains("0.12"))

    println("Hardware dashboard formatter tests PASS")
}
