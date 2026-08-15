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
    val cloudClaimToken: String = "",
    val setupAddress: String = "192.168.4.1",
    val actor: String = "admin",
    val credential: String = ""
)

enum class ProvisioningPhase {
    IDLE, QR_READY, CONNECTING_SETUP_AP, AUTHORIZING, APPLYING, WAITING_FOR_RESTART, DISCOVERING_LOCAL, COMPLETE, ERROR
}

data class ProvisioningUiState(
    val phase: ProvisioningPhase = ProvisioningPhase.IDLE,
    val qr: ProvisioningQrData? = null,
    val message: String = "Виберіть HomeGuard або відскануйте QR-код",
    val error: String = "",
    val localUrl: String = ""
)
