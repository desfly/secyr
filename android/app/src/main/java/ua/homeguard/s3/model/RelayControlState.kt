package ua.homeguard.s3.model

data class RelayControlState(
    val available: Boolean = false,
    val lightActive: Boolean = false,
    val lightManual: Boolean = false,
    val lightAutomatic: Boolean = false,
    val lockActive: Boolean = false,
    val lockRemainingMs: Int = 0,
)
