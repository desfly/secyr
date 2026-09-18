package ua.homeguard.s3.network.ble

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

data class BleRuntimeTransition(
    val index: Long,
    val stage: String,
    val detail: String = "",
    val statusCode: Int? = null,
)

data class BleRuntimeDiagnostic(
    val stage: String = "IDLE",
    val detail: String = "",
    val statusCode: Int? = null,
    val address: String = "",
    val transition: Long = 0,
    val history: List<BleRuntimeTransition> = emptyList(),
)

object BleRuntimeDiagnostics {
    private const val MAX_HISTORY = 12
    private val flow = MutableStateFlow(BleRuntimeDiagnostic())

    fun state(): StateFlow<BleRuntimeDiagnostic> = flow.asStateFlow()
    fun current(): BleRuntimeDiagnostic = flow.value

    @Synchronized
    fun update(
        stage: String,
        detail: String = "",
        statusCode: Int? = null,
        address: String = flow.value.address,
    ) {
        val previous = flow.value
        val nextIndex = previous.transition + 1
        val historyStage = if (statusCode != null) "$stage[$statusCode]" else stage
        val nextHistory = (previous.history + BleRuntimeTransition(
            index = nextIndex,
            stage = historyStage,
            detail = detail,
            statusCode = statusCode,
        )).takeLast(MAX_HISTORY)

        flow.value = BleRuntimeDiagnostic(
            stage = stage,
            detail = detail,
            statusCode = statusCode,
            address = address,
            transition = nextIndex,
            history = nextHistory,
        )
    }

    @Synchronized
    fun reset() {
        flow.value = BleRuntimeDiagnostic()
    }
}
