package ua.homeguard.s3.network.ble

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

data class BleRuntimeDiagnostic(
    val stage: String = "IDLE",
    val detail: String = "",
    val statusCode: Int? = null,
    val address: String = "",
)

object BleRuntimeDiagnostics {
    private val flow = MutableStateFlow(BleRuntimeDiagnostic())

    fun state(): StateFlow<BleRuntimeDiagnostic> = flow.asStateFlow()
    fun current(): BleRuntimeDiagnostic = flow.value

    fun update(
        stage: String,
        detail: String = "",
        statusCode: Int? = null,
        address: String = flow.value.address,
    ) {
        flow.value = BleRuntimeDiagnostic(stage, detail, statusCode, address)
    }

    fun reset() {
        flow.value = BleRuntimeDiagnostic()
    }
}
