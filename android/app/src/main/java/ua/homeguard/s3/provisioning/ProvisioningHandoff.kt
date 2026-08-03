package ua.homeguard.s3.provisioning

enum class HandoffState { IDLE, WAITING_FOR_REBOOT, DISCOVERING_LOCAL, COMPLETE, TIMED_OUT }

data class HandoffResult(
    val state: HandoffState,
    val localUrl: String = "",
    val accepted: Boolean = false
)

class ProvisioningHandoff(
    private val expectedDeviceId: String,
    private val timeoutMs: Long = 60_000L
) {
    private var state = HandoffState.IDLE
    private var deadlineMs = 0L
    private var localUrl = ""

    fun applyAccepted(nowMs: Long): HandoffResult {
        require(expectedDeviceId.isNotBlank()) { "deviceId is required" }
        state = HandoffState.WAITING_FOR_REBOOT
        deadlineMs = nowMs + timeoutMs
        return snapshot()
    }

    fun beginDiscovery(nowMs: Long): HandoffResult {
        if (state != HandoffState.WAITING_FOR_REBOOT || nowMs > deadlineMs) {
            if (deadlineMs != 0L && nowMs > deadlineMs) state = HandoffState.TIMED_OUT
            return snapshot()
        }
        state = HandoffState.DISCOVERING_LOCAL
        return snapshot()
    }

    fun observe(
        deviceId: String,
        secure: Boolean,
        pairingRequired: Boolean,
        url: String,
        nowMs: Long
    ): HandoffResult {
        if (nowMs > deadlineMs) {
            state = HandoffState.TIMED_OUT
            return snapshot()
        }
        if (state != HandoffState.DISCOVERING_LOCAL || deviceId != expectedDeviceId ||
            !secure || pairingRequired || !url.startsWith("https://")) return snapshot()
        localUrl = url
        state = HandoffState.COMPLETE
        return snapshot(accepted = true)
    }

    fun tick(nowMs: Long): HandoffResult {
        if (state != HandoffState.COMPLETE && deadlineMs != 0L && nowMs > deadlineMs) {
            state = HandoffState.TIMED_OUT
        }
        return snapshot()
    }

    private fun snapshot(accepted: Boolean = false) = HandoffResult(state, localUrl, accepted)
}
