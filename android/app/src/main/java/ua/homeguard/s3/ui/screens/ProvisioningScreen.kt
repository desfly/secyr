package ua.homeguard.s3.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.dp
import ua.homeguard.s3.model.ProvisioningForm
import ua.homeguard.s3.model.ProvisioningPhase
import ua.homeguard.s3.model.ProvisioningUiState
import ua.homeguard.s3.provisioning.ScannedWifiNetwork

private val compactField = Modifier.fillMaxWidth().heightIn(min = 56.dp)
private val compactButton = Modifier.fillMaxWidth().heightIn(min = 48.dp)

@Composable
fun ProvisioningScreen(
    state: ProvisioningUiState,
    wifiNetworks: List<ScannedWifiNetwork>,
    onScan: () -> Unit,
    onScanWifi: () -> Unit,
    onProvision: (ProvisioningForm) -> Unit,
    onCloudAttach: (deviceId: String, actor: String, pin: String) -> Unit,
) {
    var form by remember { mutableStateOf(ProvisioningForm()) }
    var existingDeviceId by remember { mutableStateOf("") }
    var existingActor by remember { mutableStateOf("admin") }
    var existingPin by remember { mutableStateOf("") }
    var showExistingPin by remember { mutableStateOf(false) }
    var showWifiPassword by remember { mutableStateOf(false) }
    var showClaimToken by remember { mutableStateOf(false) }
    val busy = state.phase in setOf(
        ProvisioningPhase.CONNECTING_SETUP_AP,
        ProvisioningPhase.AUTHORIZING,
        ProvisioningPhase.APPLYING,
        ProvisioningPhase.WAITING_FOR_RESTART,
        ProvisioningPhase.DISCOVERING_LOCAL,
    )
    val normalizedDeviceId = existingDeviceId.trim().uppercase()
    val primaryText = MaterialTheme.colorScheme.onBackground
    val secondaryText = MaterialTheme.colorScheme.onSurfaceVariant

    Column(
        modifier = Modifier
            .fillMaxSize()
            .statusBarsPadding()
            .navigationBarsPadding()
            .verticalScroll(rememberScrollState())
            .padding(horizontal = 14.dp, vertical = 8.dp),
        verticalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        Text("Myfist", style = MaterialTheme.typography.headlineMedium, fontWeight = FontWeight.Black, color = MaterialTheme.colorScheme.primary)
        Text("HomeGuard-S3 · локальна та хмарна безпека", style = MaterialTheme.typography.bodyMedium, color = secondaryText)

        Text("Вже налаштований HomeGuard", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold, color = primaryText)
        Text("Підключення через хмару без повторного налаштування Wi-Fi.", style = MaterialTheme.typography.bodyMedium, color = secondaryText)

        OutlinedTextField(existingDeviceId, { existingDeviceId = it.trim().uppercase().take(40) }, label = { Text("Device ID") }, placeholder = { Text("HG-XXXXXXXXXXXX") }, modifier = compactField, singleLine = true)
        OutlinedTextField(existingActor, { existingActor = it.trim().take(23) }, label = { Text("Користувач") }, modifier = compactField, singleLine = true)
        OutlinedTextField(
            existingPin,
            { existingPin = it.take(12) },
            label = { Text("PIN") },
            modifier = compactField,
            visualTransformation = if (showExistingPin) VisualTransformation.None else PasswordVisualTransformation(),
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.NumberPassword),
            trailingIcon = { IconButton(onClick = { showExistingPin = !showExistingPin }) { Text(if (showExistingPin) "◉" else "👁") } },
            singleLine = true
        )
        Button(onClick = { onCloudAttach(normalizedDeviceId, existingActor.trim(), existingPin) }, enabled = !busy && normalizedDeviceId.startsWith("HG-") && existingActor.isNotBlank() && existingPin.length in 4..12, modifier = compactButton) { Text("Підключити через хмару") }

        HorizontalDivider(modifier = Modifier.padding(vertical = 3.dp))
        Text("Новий HomeGuard — перше налаштування", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold, color = MaterialTheme.colorScheme.secondary)
        Text(state.message, style = MaterialTheme.typography.bodyMedium, color = secondaryText)
        if (state.error.isNotBlank()) Text("Помилка: ${state.error}", color = MaterialTheme.colorScheme.error)
        if (state.localUrl.isNotBlank()) Text("Локальна адреса: ${state.localUrl}", style = MaterialTheme.typography.bodySmall, color = secondaryText)
        Button(onClick = onScan, enabled = !busy, modifier = compactButton) { Text("Сканувати QR на пристрої") }
        state.qr?.let {
            Text("Пристрій: ${it.deviceId}", style = MaterialTheme.typography.bodySmall, color = primaryText)
            Text("Setup AP: ${it.setupSsid}", style = MaterialTheme.typography.bodySmall, color = secondaryText)
            Button(onClick = onScanWifi, enabled = !busy, modifier = compactButton) { Text("Сканувати Wi-Fi") }
        }
        if (wifiNetworks.isNotEmpty()) {
            Text("Доступні Wi-Fi мережі", style = MaterialTheme.typography.titleSmall, color = primaryText)
            wifiNetworks.take(12).forEach { network ->
                Button(onClick = { form = form.copy(wifiSsid = network.ssid) }, enabled = !busy, modifier = compactButton) { Text("${network.ssid}  ${network.rssi} dBm  ch ${network.channel}") }
            }
        }
        OutlinedTextField(form.wifiSsid, { form = form.copy(wifiSsid = it) }, label = { Text("Домашня Wi-Fi мережа") }, modifier = compactField, singleLine = true)
        OutlinedTextField(
            form.wifiPassword,
            { form = form.copy(wifiPassword = it) },
            label = { Text("Пароль домашнього Wi-Fi") },
            modifier = compactField,
            visualTransformation = if (showWifiPassword) VisualTransformation.None else PasswordVisualTransformation(),
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password),
            trailingIcon = { IconButton(onClick = { showWifiPassword = !showWifiPassword }) { Text(if (showWifiPassword) "◉" else "👁") } },
            singleLine = true
        )
        OutlinedTextField(form.ownerLabel, { form = form.copy(ownerLabel = it) }, label = { Text("Назва об’єкта") }, modifier = compactField, singleLine = true)
        OutlinedTextField(form.cloudEndpoint, { form = form.copy(cloudEndpoint = it) }, label = { Text("MQTTS адреса хмари — необов’язково") }, modifier = compactField, singleLine = true)
        OutlinedTextField(
            form.cloudClaimToken,
            { form = form.copy(cloudClaimToken = it) },
            label = { Text("Одноразовий cloud claim token") },
            modifier = compactField,
            visualTransformation = if (showClaimToken) VisualTransformation.None else PasswordVisualTransformation(),
            trailingIcon = { IconButton(onClick = { showClaimToken = !showClaimToken }) { Text(if (showClaimToken) "◉" else "👁") } },
            singleLine = true
        )
        Button(onClick = { onProvision(form) }, enabled = state.qr != null && !busy, modifier = compactButton) { Text("Прив’язати HomeGuard-S3") }
        if (busy) CircularProgressIndicator()
    }
}
