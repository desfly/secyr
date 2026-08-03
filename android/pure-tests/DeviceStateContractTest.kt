import ua.homeguard.s3.api.model.DeviceStateDto
import ua.homeguard.s3.api.model.ValveDto
import ua.homeguard.s3.api.model.ZoneDto

fun main() {
    val state = DeviceStateDto(
        sequence = "42",
        serverTimeMs = "1000",
        securityMode = "armed_away",
        corridorLight = true,
        siren = false,
        mainsPresent = true,
        batteryVoltageV = 12.4,
        batteryCurrentA = -0.2,
        coldPressureBar = 4.8,
        hotPressureBar = 4.5,
        coldTemperatureC = 12.0,
        hotTemperatureC = 48.0,
        zones = listOf(
            ZoneDto("motion", "Motion", "normal", false, false, 1650.0)
        ),
        valves = listOf(
            ValveDto("cold", "closed", false, 0)
        ),
    )

    check(state.sequence == "42")
    check(state.securityMode == "armed_away")
    check(state.zones.first().id == "motion")
    check(state.valves.first().state == "closed")
    println("Build-0028 Android device contract tests PASS")
}
