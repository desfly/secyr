package ua.homeguard.s3.model

enum class HardwareModuleState {
    NOT_INITIALIZED,
    READY,
    DEGRADED,
    MISSING,
    FAULT,
}

data class HardwareModuleHealth(
    val name: String,
    val state: HardwareModuleState,
    val detail: String,
    val errorCount: Long,
)

data class HardwareRuntimeHealth(
    val modules: List<HardwareModuleHealth>,
    val safeOutputsForced: Boolean,
)
