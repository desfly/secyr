package ua.homeguard.s3.diagnostics

import org.json.JSONObject

data class ServiceReadiness(
    val status: String,
    val outputsAllowed: Boolean,
    val hardwareRecordPresent: Boolean,
    val hardwareRecordValid: Boolean,
    val commissioningRecordPresent: Boolean,
    val commissioningRecordValid: Boolean,
) {
    val readyForPhysicalOutputs: Boolean
        get() = outputsAllowed && hardwareRecordValid && commissioningRecordValid

    companion object {
        fun fromJson(json: String): ServiceReadiness {
            val o = JSONObject(json)
            return ServiceReadiness(
                status = o.optString("status", "unknown"),
                outputsAllowed = o.optBoolean("outputsAllowed", false),
                hardwareRecordPresent = o.optBoolean("hardwareRecordPresent", false),
                hardwareRecordValid = o.optBoolean("hardwareRecordValid", false),
                commissioningRecordPresent = o.optBoolean("commissioningRecordPresent", false),
                commissioningRecordValid = o.optBoolean("commissioningRecordValid", false),
            )
        }
    }
}
