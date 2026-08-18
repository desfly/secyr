package ua.homeguard.s3.storage

import org.junit.Assert.assertEquals
import org.junit.Test

class SettingsSecretPolicyTest {
    @Test
    fun sameDeviceRestorePreservesMissingTelemetryToken() {
        val current = AppSettings(
            deviceId = "HG-1",
            apiToken = "api-a",
            telemetryToken = "telemetry-a",
        )
        val restored = current.copy(telemetryToken = "")

        assertEquals(
            DeviceBoundSecrets("api-a", "telemetry-a"),
            normalizeDeviceBoundSecrets(current, restored),
        )
    }

    @Test
    fun normalDeviceSwitchDoesNotCarryOldSecrets() {
        val current = AppSettings(
            deviceId = "HG-1",
            apiToken = "api-a",
            telemetryToken = "telemetry-a",
        )
        val next = current.copy(deviceId = "HG-2")

        assertEquals(
            DeviceBoundSecrets("", ""),
            normalizeDeviceBoundSecrets(current, next),
        )
    }

    @Test
    fun provisioningMaySetNewTokenWhileSelectingNewDevice() {
        val current = AppSettings(
            deviceId = "HG-1",
            apiToken = "api-a",
            telemetryToken = "telemetry-a",
        )
        val provisioned = AppSettings(
            deviceId = "HG-2",
            apiToken = "api-b",
            telemetryToken = "",
        )

        assertEquals(
            DeviceBoundSecrets("api-b", ""),
            normalizeDeviceBoundSecrets(current, provisioned),
        )
    }

    @Test
    fun deviceIdCaseChangeDoesNotClearSecrets() {
        val current = AppSettings(
            deviceId = "HG-ABC",
            apiToken = "api-a",
            telemetryToken = "telemetry-a",
        )
        val next = current.copy(deviceId = "hg-abc", telemetryToken = "")

        assertEquals(
            DeviceBoundSecrets("api-a", "telemetry-a"),
            normalizeDeviceBoundSecrets(current, next),
        )
    }
}
