package ua.homeguard.s3.model

data class ProvisioningQrData(
    val version: Int,
    val deviceId: String,
    val setupSsid: String,
    val setupPassword: String,
    val setupUrl: String,
    val certificateSha256: String,
    val pairingCode: String
)

data class ProvisioningForm(
    val wifiSsid: String = "",
    val wifiPassword: String = "",
    val ownerLabel: String = "",
    val cloudEndpoint: String = "",
    val cloudClaimToken: String = ""
)

enum class ProvisioningPhase {
    IDLE, QR_READY, CONNECTING_SETUP_AP, AUTHORIZING, APPLYING, WAITING_FOR_RESTART, DISCOVERING_LOCAL, COMPLETE, ERROR
}

data class ProvisioningUiState(
    val phase: ProvisioningPhase = ProvisioningPhase.IDLE,
    val qr: ProvisioningQrData? = null,
    val message: String = "Відскануйте QR-код HomeGuard-S3",
    val error: String = "",
    val localUrl: String = ""
)
