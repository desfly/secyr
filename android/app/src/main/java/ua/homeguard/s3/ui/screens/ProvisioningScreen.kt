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
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
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
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.model.ProvisioningForm
import ua.homeguard.s3.model.ProvisioningPhase
import ua.homeguard.s3.model.ProvisioningUiState
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
    var form by remember { mutableStateOf(ProvisioningForm()) }
    var manualAddress by remember { mutableStateOf("192.168.4.1") }
    val busy = state.phase in setOf(
        ProvisioningPhase.CONNECTING_SETUP_AP,
        ProvisioningPhase.AUTHORIZING,
        ProvisioningPhase.APPLYING,
        ProvisioningPhase.WAITING_FOR_RESTART,
        ProvisioningPhase.DISCOVERING_LOCAL,
    )
    val manualAddressValid = normalizeLocalAddress(manualAddress) != null

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
        Text("1. Ідентифікуйте новий контролер через QR. 2. Введіть домашню Wi-Fi мережу та пароль. 3. Передайте налаштування і дочекайтеся появи HomeGuard у LAN.")

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
            visualTransformation = PasswordVisualTransformation(),
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
            visualTransformation = PasswordVisualTransformation(),
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
            OutlinedButton(onClick = { onUseDevice(device) }, enabled = !busy, modifier = Modifier.fillMaxWidth()) {
                Text("${device.serviceName.ifBlank { "HomeGuard-S3" }} · ${device.host}:${device.port}")
            }
        }

        HorizontalDivider()
        Text("Ручне підключення по IP")
        OutlinedTextField(
            value = manualAddress,
            onValueChange = { manualAddress = it },
            label = { Text("IP або IP:порт") },
            supportingText = { Text("Наприклад: 192.168.4.1") },
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
