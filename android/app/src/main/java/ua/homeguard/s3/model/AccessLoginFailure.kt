package ua.homeguard.s3.model

import java.io.IOException

enum class AccessLoginFailureReason {
    USER_NOT_FOUND,
    ACCESS_REVOKED,
    BAD_CREDENTIALS,
    UNKNOWN,
}

class AccessLoginRejectedException(
    val reason: AccessLoginFailureReason,
    message: String,
) : IOException(message)

fun accessLoginFailureReason(raw: String): AccessLoginFailureReason {
    val normalized = raw.trim().uppercase().replace('-', '_').replace(' ', '_')
    return when {
        normalized.contains("USER_NOT_FOUND") || normalized.contains("UNKNOWN_USER") -> AccessLoginFailureReason.USER_NOT_FOUND
        normalized.contains("ACCESS_REVOKED") || normalized.contains("REVOKED") || normalized.contains("USER_DISABLED") -> AccessLoginFailureReason.ACCESS_REVOKED
        normalized.contains("BAD_CREDENTIAL") || normalized.contains("INVALID_CREDENTIAL") || normalized.contains("BAD_PASSWORD") || normalized.contains("BAD_PIN") -> AccessLoginFailureReason.BAD_CREDENTIALS
        else -> AccessLoginFailureReason.UNKNOWN
    }
}
