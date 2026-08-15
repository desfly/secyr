package ua.homeguard.s3.ui.screens

import android.app.Activity
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.model.ProvisioningForm
import ua.homeguard.s3.model.ProvisioningPhase
import ua.homeguard.s3.model.ProvisioningUiState
import ua.homeguard.s3.network.HomeGuardWifiNetwork
import ua.homeguard.s3.network.HomeGuardWifiScanner
import ua.homeguard.s3.network.LocalDiscoveryCoordinator
import ua.homeguard.s3.network.UdpDeviceDiscovery
import ua.homeguard.s3.storage.SettingsStore
import java.net.URI

@Composable
fun ProvisioningScreen(
    state: ProvisioningUiState,
    onBack: () -> Unit,
    onScan: () -> Unit,
    onProvision: (ProvisioningForm) -> Unit,
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val discovery = remember(context) { LocalDiscoveryCoordinator(context, scope) }
    val localSettings = remember(context) { SettingsStore(context) }
    val devices by discovery.devices.collectAsState()
    val isScanning by discovery.isScanning.collectAsState()
    val scanStatus by discovery.scanStatus.collectAsState()

    DisposableEffect(discovery) {
        discovery.start()
        onDispose { discovery.stop() }
    }

    fun finishManualSelection(address: String) {
        val endpoint = normalizeLocalAddress(address) ?: return
        val uri = runCatching { URI(endpoint) }.getOrNull() ?: return
        val host = uri.host ?: return
        val port = when {
            uri.port in 1..65535 -> uri.port
            uri.scheme.equals("https", ignoreCase = true) -> 443
            else -> 80
        }
        scope.launch {
            localSettings.selectDevice("manual-${host.lowercase()}-$port", endpoint)
            (context as? Activity)?.recreate()
        }
    }

    ProvisioningScreen(
        state = state,
        devices = devices,
        isScanningNetwork = isScanning,
        scanStatus = scanStatus,
        onBack = onBack,
        onScanQr = onScan,
        onDiscover = { scope.launch { discovery.rescan() } },
        onUseDevice = { device ->
            scope.launch {
                localSettings.remember(device)
                (context as? Activity)?.recreate()
            }
        },
        onUseManualIp = ::finishManualSelection,
        onProvision = onProvision,
    )
}

@Composable
fun ProvisioningScreen(
    state: ProvisioningUiState,
    devices: List<DiscoveredDevice>,
    isScanningNetwork: Boolean,
    scanStatus: UdpDeviceDiscovery.ScanStatus,
    onBack: () -> Unit,
    onScanQr: () -> Unit,
    onDiscover: () -> Unit,
    onUseDevice: (DiscoveredDevice) -> Unit,
    onUseManualIp: (String) -> Unit,
    onProvision: (ProvisioningForm) -> Unit,
) {
    val scope = rememberCoroutineScope()
    var form by remember { mutableStateOf(ProvisioningForm()) }
    var manualAddress by remember { mutableStateOf("192.168.4.1") }
    var manualAddressTouched by remember { mutableStateOf(false) }
    var wifiPasswordVisible by remember { mutableStateOf(false) }
    var cloudTokenVisible by remember { mutableStateOf(false) }
    var wifiScanBusy by remember { mutableStateOf(false) }
    var wifiScanError by remember { mutableStateOf("") }
    var wifiNetworks by remember { mutableStateOf(emptyList<HomeGuardWifiNetwork>()) }
    val busy = state.phase in setOf(
        ProvisioningPhase.CONNECTING_SETUP_AP,
        ProvisioningPhase.AUTHORIZING,
        ProvisioningPhase.APPLYING,
        ProvisioningPhase.WAITING_FOR_RESTART,
        ProvisioningPhase.DISCOVERING_LOCAL,
    )
    val manualAddressValid = normalizeLocalAddress(manualAddress) != null

    LaunchedEffect(devices) {
        if (!manualAddressTouched && devices.isNotEmpty()) {
            val first = devices.first()
            manualAddress = if (first.port == 80) first.host else "${first.host}:${first.port}"
        }
    }

    fun scanWifiOnHomeGuard() {
        if (!manualAddressValid || wifiScanBusy) return
        scope.launch {
            wifiScanBusy = true
            wifiScanError = ""
            runCatching { HomeGuardWifiScanner.scan(manualAddress) }
                .onSuccess { networks ->
                    wifiNetworks = networks
                    if (networks.isEmpty()) wifiScanError = "HomeGuard не знайшов Wi-Fi мереж"
                }
                .onFailure { error ->
                    wifiNetworks = emptyList()
                    wifiScanError = error.message ?: "Не вдалося запустити сканування на HomeGuard"
                }
            wifiScanBusy = false
        }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(20.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        OutlinedButton(onClick = onBack, enabled = !busy, modifier = Modifier.fillMaxWidth()) {
            Text("← Назад")
        }
        Text("Підключити HomeGuard до Wi-Fi")
        Text("1. Ідентифікуйте новий контролер через QR. 2. Виберіть домашню Wi-Fi мережу зі скану HomeGuard або введіть SSID вручну. 3. Передайте налаштування і дочекайтеся появи HomeGuard у LAN.")

        HorizontalDivider()
        Text("Крок 1 — вибрати новий HomeGuard")
        OutlinedButton(onClick = onScanQr, enabled = !busy, modifier = Modifier.fillMaxWidth()) {
            Text("Сканувати QR HomeGuard")
        }
        state.qr?.let {
            Text("Пристрій: ${it.deviceId}")
            Text("Setup AP: ${it.setupSsid}")
        }

        HorizontalDivider()
        Text("Крок 2 — домашня Wi-Fi мережа")
        OutlinedButton(
            onClick = ::scanWifiOnHomeGuard,
            enabled = manualAddressValid && !busy && !wifiScanBusy,
            modifier = Modifier.fillMaxWidth(),
        ) {
            Text(if (wifiScanBusy) "HomeGuard сканує Wi-Fi…" else "Сканувати Wi-Fi через HomeGuard")
        }
        Text("Сканування виконує ESP32-S3 за адресою ${normalizeLocalAddress(manualAddress) ?: manualAddress}.")
        if (wifiScanBusy) {
            Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                CircularProgressIndicator()
                Text("Очікування результатів сканування ESP…")
            }
        }
        if (wifiScanError.isNotBlank()) Text("Wi-Fi scan: $wifiScanError")
        wifiNetworks.forEach { network ->
            OutlinedButton(
                onClick = { form = form.copy(wifiSsid = network.ssid) },
                enabled = !busy && !wifiScanBusy,
                modifier = Modifier.fillMaxWidth(),
            ) {
                Text("${network.ssid} · ${network.rssi} dBm")
            }
        }
        OutlinedTextField(
            form.wifiSsid,
            { form = form.copy(wifiSsid = it) },
            label = { Text("Назва Wi-Fi (SSID)") },
            modifier = Modifier.fillMaxWidth(),
        )
        OutlinedTextField(
            form.wifiPassword,
            { form = form.copy(wifiPassword = it) },
            label = { Text("Пароль Wi-Fi") },
            modifier = Modifier.fillMaxWidth(),
            visualTransformation = if (wifiPasswordVisible) VisualTransformation.None else PasswordVisualTransformation(),
            trailingIcon = {
                TextButton(onClick = { wifiPasswordVisible = !wifiPasswordVisible }) {
                    Text(if (wifiPasswordVisible) "Сховати" else "Показати")
                }
            },
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password),
        )
        OutlinedTextField(
            form.ownerLabel,
            { form = form.copy(ownerLabel = it) },
            label = { Text("Назва пристрою/об’єкта") },
            modifier = Modifier.fillMaxWidth(),
        )

        HorizontalDivider()
        Text("Крок 3 — Internet/Cloud")
        Text("Ці поля потрібні лише якщо для цього HomeGuard увімкнено віддалений доступ.")
        OutlinedTextField(
            form.cloudEndpoint,
            { form = form.copy(cloudEndpoint = it) },
            label = { Text("MQTTS адреса хмари — необов’язково") },
            modifier = Modifier.fillMaxWidth(),
        )
        OutlinedTextField(
            form.cloudClaimToken,
            { form = form.copy(cloudClaimToken = it) },
            label = { Text("Одноразовий cloud claim token") },
            modifier = Modifier.fillMaxWidth(),
            visualTransformation = if (cloudTokenVisible) VisualTransformation.None else PasswordVisualTransformation(),
            trailingIcon = {
                TextButton(onClick = { cloudTokenVisible = !cloudTokenVisible }) {
                    Text(if (cloudTokenVisible) "Сховати" else "Показати")
                }
            },
        )
        Button(
            onClick = { onProvision(form) },
            enabled = state.qr != null && form.wifiSsid.isNotBlank() && !busy,
            modifier = Modifier.fillMaxWidth(),
        ) {
            Text("Передати Wi-Fi налаштування в HomeGuard")
        }
        Text(state.message)
        if (state.error.isNotBlank()) Text("Помилка: ${state.error}")
        if (state.localUrl.isNotBlank()) Text("Локальна адреса: ${state.localUrl}")
        if (busy) CircularProgressIndicator()

        HorizontalDivider()
        Text("Якщо HomeGuard уже підключений до Wi-Fi")
        Button(
            onClick = onDiscover,
            enabled = !isScanningNetwork && !busy,
            modifier = Modifier.fillMaxWidth(),
        ) {
            Text(if (isScanningNetwork) "Пошук у мережі…" else "Знайти у локальній мережі")
        }
        if (isScanningNetwork) {
            Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                CircularProgressIndicator()
                Text("UDP/mDNS: ${scanStatus.phase} · ${(scanStatus.progress * 100f).toInt().coerceIn(0, 100)}%")
            }
        } else {
            Text("Знайдено пристроїв: ${devices.size}")
        }
        if (scanStatus.phase != "idle") {
            Text("UDP: надіслано ${scanStatus.sent} · отримано ${scanStatus.received} · прийнято ${scanStatus.accepted}")
            if (scanStatus.error.isNotBlank()) Text("Діагностика: ${scanStatus.error}")
        }
        devices.forEach { device ->
            OutlinedButton(
                onClick = {
                    manualAddressTouched = false
                    manualAddress = if (device.port == 80) device.host else "${device.host}:${device.port}"
                    onUseDevice(device)
                },
                enabled = !busy,
                modifier = Modifier.fillMaxWidth(),
            ) {
                Text("${device.serviceName.ifBlank { "HomeGuard-S3" }} · ${device.host}:${device.port}")
            }
        }

        HorizontalDivider()
        Text("Ручне підключення по IP")
        OutlinedTextField(
            value = manualAddress,
            onValueChange = {
                manualAddressTouched = true
                manualAddress = it.trim()
            },
            label = { Text("IP або IP:порт") },
            supportingText = {
                Text(
                    if (devices.isNotEmpty()) "Автозаповнено зі знайденого HomeGuard"
                    else "За замовчуванням: 192.168.4.1"
                )
            },
            singleLine = true,
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Uri),
            modifier = Modifier.fillMaxWidth(),
        )
        Button(
            onClick = { onUseManualIp(manualAddress) },
            enabled = manualAddressValid && !busy,
            modifier = Modifier.fillMaxWidth(),
        ) {
            Text("Підключити по IP")
        }
    }
}

private fun normalizeLocalAddress(raw: String): String? {
    val value = raw.trim().trimEnd('/')
    if (value.isEmpty() || value.any(Char::isWhitespace)) return null
    val withScheme = when {
        value.startsWith("http://", ignoreCase = true) || value.startsWith("https://", ignoreCase = true) -> value
        else -> "http://$value"
    }
    val uri = runCatching { URI(withScheme) }.getOrNull() ?: return null
    val scheme = uri.scheme?.lowercase()
    if (scheme != "http" && scheme != "https") return null
    if (uri.host.isNullOrBlank()) return null
    if (uri.port == 0 || uri.port > 65535) return null
    if (!uri.path.isNullOrBlank() && uri.path != "/") return null
    if (!uri.query.isNullOrBlank() || !uri.fragment.isNullOrBlank() || !uri.userInfo.isNullOrBlank()) return null
    return withScheme.trimEnd('/')
}
