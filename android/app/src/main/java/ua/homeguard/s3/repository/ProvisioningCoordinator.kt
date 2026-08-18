package ua.homeguard.s3.repository

import android.content.Context
import android.util.Base64
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.filterNotNull
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeout
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.model.ProvisioningForm
import ua.homeguard.s3.model.ProvisioningPhase
import ua.homeguard.s3.model.ProvisioningUiState
import ua.homeguard.s3.network.LocalDiscoveryCoordinator
import ua.homeguard.s3.provisioning.PinnedProvisioningApi
import ua.homeguard.s3.provisioning.ProvisioningHandoff
import ua.homeguard.s3.provisioning.ProvisioningQrParser
import ua.homeguard.s3.provisioning.SetupNetworkConnector
import ua.homeguard.s3.storage.AppSettings
import ua.homeguard.s3.storage.RegisteredDeviceStore
import ua.homeguard.s3.storage.SettingsStore
import java.security.SecureRandom

class ProvisioningCoordinator(
    context: Context,
    private val settings: SettingsStore,
    private val discovery: LocalDiscoveryCoordinator,
    private val registeredDevices: RegisteredDeviceStore,
    private val scope: CoroutineScope,
) {
    private val connector = SetupNetworkConnector(context)
    private val mutableState = MutableStateFlow(ProvisioningUiState())
    val state: StateFlow<ProvisioningUiState> = mutableState

    fun acceptQr(raw: String) {
        runCatching { ProvisioningQrParser.parse(raw) }
            .onSuccess {
                mutableState.value = ProvisioningUiState(
                    ProvisioningPhase.QR_READY,
                    it,
                    "QR перевірено: ${it.deviceId}"
                )
            }
            .onFailure {
                mutableState.value = ProvisioningUiState(
                    ProvisioningPhase.ERROR,
                    error = it.message.orEmpty(),
                    message = "QR відхилено"
                )
            }
    }

    fun provision(form: ProvisioningForm) {
        val qr = mutableState.value.qr ?: return
        scope.launch {
            runCatching {
                val ownerLabel = form.ownerLabel.trim().take(40)
                require(ownerLabel.isNotBlank()) { "Вкажіть назву пристрою перед збереженням" }
                require(form.wifiSsid.isNotBlank()) { "Вкажіть домашню Wi-Fi мережу" }
                require(form.wifiPassword.length in 8..64) { "Перевірте пароль домашньої Wi-Fi мережі" }
                val localApiToken = randomToken()
                val handoff = ProvisioningHandoff(qr.deviceId, 60_000L)

                mutableState.value = mutableState.value.copy(
                    phase = ProvisioningPhase.CONNECTING_SETUP_AP,
                    message = "Підключення до ${qr.setupSsid}",
                    error = ""
                )
                connector.connect(qr.setupSsid, qr.setupPassword).use {
                    val api = PinnedProvisioningApi(qr)
                    mutableState.value = mutableState.value.copy(
                        phase = ProvisioningPhase.AUTHORIZING,
                        message = "Перевірка одноразового коду"
                    )
                    api.authorize()
                    mutableState.value = mutableState.value.copy(
                        phase = ProvisioningPhase.APPLYING,
                        message = "Передавання налаштувань через HTTPS"
                    )
                    api.apply(form, localApiToken)
                    registeredDevices.addManual(qr.deviceId, "", ownerLabel)
                    settings.update(
                        AppSettings(
                            deviceId = qr.deviceId,
                            apiToken = localApiToken,
                            autoReconnect = true,
                            remoteAccessEnabled = form.cloudEndpoint.isNotBlank(),
                            cloudBaseUrl = "",
                            lastKnownLocalUrl = "",
                            localCertificateSha256 = qr.certificateSha256
                        )
                    )
                }

                handoff.applyAccepted(System.currentTimeMillis())
                mutableState.value = mutableState.value.copy(
                    phase = ProvisioningPhase.WAITING_FOR_RESTART,
                    message = "Контролер перезапускається у режимі домашньої Wi-Fi мережі"
                )
                delay(1_500L)
                handoff.beginDiscovery(System.currentTimeMillis())
                mutableState.value = mutableState.value.copy(
                    phase = ProvisioningPhase.DISCOVERING_LOCAL,
                    message = "Пошук ${qr.deviceId} через mDNS та UDP"
                )

                val device = runCatching { awaitLocalDevice(qr.deviceId, 60_000L) }.getOrNull()
                if (device != null) {
                    val accepted = handoff.observe(
                        device.deviceId,
                        device.secure,
                        device.pairingRequired,
                        device.baseUrl,
                        System.currentTimeMillis()
                    )
                    require(accepted.accepted) { "Знайдений пристрій не пройшов перевірку handoff" }
                    registeredDevices.refreshDiscovered(device)
                    settings.remember(device)
                    mutableState.value = mutableState.value.copy(
                        phase = ProvisioningPhase.COMPLETE,
                        message = "HomeGuard-S3 підключено до домашньої мережі",
                        localUrl = device.baseUrl
                    )
                } else {
                    handoff.tick(System.currentTimeMillis() + 60_001L)
                    mutableState.value = mutableState.value.copy(
                        phase = ProvisioningPhase.COMPLETE,
                        message = "Пристрій прив’язано, але локально ще не знайдено; пошук продовжиться автоматично"
                    )
                }
            }.onFailure {
                mutableState.value = mutableState.value.copy(
                    phase = ProvisioningPhase.ERROR,
                    message = "Налаштування не завершено",
                    error = it.message.orEmpty()
                )
            }
        }
    }

    private suspend fun awaitLocalDevice(deviceId: String, timeoutMs: Long): DiscoveredDevice {
        discovery.stop()
        discovery.start()
        discovery.rescan()
        return withTimeout(timeoutMs) {
            discovery.devices
                .map { devices ->
                    devices.firstOrNull {
                        it.deviceId.trim().equals(deviceId.trim(), ignoreCase = true) &&
                            it.secure && !it.pairingRequired
                    }
                }
                .filterNotNull()
                .first()
        }
    }

    private fun randomToken(): String {
        val bytes = ByteArray(48).also(SecureRandom()::nextBytes)
        return Base64.encodeToString(bytes, Base64.NO_WRAP or Base64.URL_SAFE or Base64.NO_PADDING)
    }
}
