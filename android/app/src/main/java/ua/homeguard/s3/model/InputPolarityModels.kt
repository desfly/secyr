package ua.homeguard.s3.model

enum class InputPolarity(val wireValue: String) {
    UNKNOWN("unknown"),
    ACTIVE_HIGH("active_high"),
    ACTIVE_LOW("active_low");

    companion object {
        fun fromWire(value: String?): InputPolarity = when (value?.lowercase()) {
            "active_high" -> ACTIVE_HIGH
            "active_low" -> ACTIVE_LOW
            else -> UNKNOWN
        }
    }
}

data class InputPolarityConfig(
    val tamper: InputPolarity = InputPolarity.UNKNOWN,
    val powerFail: InputPolarity = InputPolarity.UNKNOWN,
)
