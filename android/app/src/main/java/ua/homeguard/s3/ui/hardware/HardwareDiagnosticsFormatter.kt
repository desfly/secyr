package ua.homeguard.s3.ui.hardware

import ua.homeguard.s3.model.HardwareModuleHealth
import ua.homeguard.s3.model.HardwareModuleState

object HardwareDiagnosticsFormatter {
    fun title(module: HardwareModuleHealth): String =
        "${module.name}: ${module.state.name}"

    fun requiresAttention(module: HardwareModuleHealth): Boolean =
        module.state == HardwareModuleState.MISSING ||
            module.state == HardwareModuleState.FAULT ||
            module.state == HardwareModuleState.DEGRADED

    fun safeOutputsText(safe: Boolean): String =
        if (safe) {
            "Виходи примусово вимкнені: безпечно"
        } else {
            "УВАГА: безпечний стан виходів не підтверджено"
        }
}
