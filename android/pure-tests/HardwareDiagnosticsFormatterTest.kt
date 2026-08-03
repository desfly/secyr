import ua.homeguard.s3.model.HardwareModuleHealth
import ua.homeguard.s3.model.HardwareModuleState
import ua.homeguard.s3.ui.hardware.HardwareDiagnosticsFormatter

fun main() {
    val ready = HardwareModuleHealth(
        name = "ADS1115 #1",
        state = HardwareModuleState.READY,
        detail = "0x48",
        errorCount = 0,
    )
    check(!HardwareDiagnosticsFormatter.requiresAttention(ready))
    check(HardwareDiagnosticsFormatter.title(ready).contains("READY"))

    val missing = ready.copy(
        state = HardwareModuleState.MISSING,
        errorCount = 1,
    )
    check(HardwareDiagnosticsFormatter.requiresAttention(missing))
    check(
        HardwareDiagnosticsFormatter.safeOutputsText(true)
            .contains("безпечно")
    )
    check(
        HardwareDiagnosticsFormatter.safeOutputsText(false)
            .contains("УВАГА")
    )

    println("Hardware diagnostics formatter tests PASS")
}
