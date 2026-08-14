package ua.homeguard.s3.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.network.UdpDeviceDiscovery

@Composable
fun AddDeviceScreen(
    devices: List<DiscoveredDevice>,
    isScanning: Boolean,
    scanStatus: UdpDeviceDiscovery.ScanStatus,
    onBack: () -> Unit,
    onRescan: () -> Unit,
    onUseDevice: (DiscoveredDevice) -> Unit,
    onUseManualAddress: (String, String) -> Unit,
) {
    var manualExpanded by remember { mutableStateOf(false) }
    var manualAddress by remember { mutableStateOf("") }
    var manualName by remember { mutableStateOf("HomeGuard") }
    val progress = scanStatus.progress.coerceIn(0f, 1f)

    Column(
        modifier = Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(20.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        OutlinedButton(onClick = onBack) { Text("← Назад") }
        Text("Додати пристрій", style = MaterialTheme.typography.headlineMedium)
        Text("Пошук пристроїв у локальній мережі", style = MaterialTheme.typography.titleLarge)
        Text("Переконайтеся, що HomeGuard увімкнений і підключений до цієї Wi-Fi мережі.")

        Card(modifier = Modifier.fillMaxWidth()) {
            Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Text(
                    if (isScanning) "Пошук пристроїв…" else if (scanStatus.phase == "done") "Пошук завершено" else "Готово до пошуку",
                    style = MaterialTheme.typography.titleMedium,
                )
                Text(
                    when (scanStatus.phase) {
                        "sending" -> "Надсилаємо запити"
                        "listening" -> "Слухаємо відповіді"
                        "done" -> "Результати оновлено"
                        "error" -> "Помилка пошуку"
                        else -> "UDP + mDNS"
                    }
                )
                Text("${(progress * 100f).toInt()}%", style = MaterialTheme.typography.headlineSmall)
                LinearProgressIndicator(progress = { progress }, modifier = Modifier.fillMaxWidth())
                Text("Надіслано ${scanStatus.sent} · отримано ${scanStatus.received} · прийнято ${scanStatus.accepted}")
                if (scanStatus.network.isNotBlank()) Text("Мережа: ${scanStatus.network}")
                if (scanStatus.error.isNotBlank()) Text("Діагностика: ${scanStatus.error}")

                Button(onClick = onRescan, enabled = !isScanning, modifier = Modifier.fillMaxWidth()) {
                    Text(if (isScanning) "Пошук виконується…" else "Шукати знову")
                }
            }
        }

        Text("Знайдені пристрої", style = MaterialTheme.typography.titleLarge)
        if (devices.isEmpty()) {
            Text(if (isScanning) "Очікуємо відповіді HomeGuard…" else "Пристроїв поки не знайдено")
        } else {
            devices.forEach { device ->
                OutlinedButton(onClick = { onUseDevice(device) }, modifier = Modifier.fillMaxWidth()) {
                    Column(modifier = Modifier.fillMaxWidth()) {
                        Text(device.serviceName.ifBlank { "HomeGuard" }, style = MaterialTheme.typography.titleMedium)
                        Text("${device.host}:${device.port} · ${if (device.secure) "HTTPS" else "HTTP"}")
                    }
                }
            }
        }

        OutlinedButton(onClick = { manualExpanded = !manualExpanded }, modifier = Modifier.fillMaxWidth()) {
            Text(if (manualExpanded) "Сховати ручне додавання" else "+ Додати вручну")
        }

        if (manualExpanded) {
            OutlinedTextField(
                value = manualName,
                onValueChange = { manualName = it.take(40) },
                label = { Text("Назва пристрою") },
                singleLine = true,
                modifier = Modifier.fillMaxWidth(),
            )
            OutlinedTextField(
                value = manualAddress,
                onValueChange = { manualAddress = it.trim() },
                label = { Text("IP або IP:порт") },
                supportingText = { Text("Наприклад: 192.168.1.45") },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Uri),
                singleLine = true,
                modifier = Modifier.fillMaxWidth(),
            )
            Button(
                onClick = { onUseManualAddress(manualName.trim().ifBlank { "HomeGuard" }, manualAddress) },
                enabled = manualAddress.isNotBlank(),
                modifier = Modifier.fillMaxWidth(),
            ) { Text("Додати") }
        }

        Text("ID пристрою у списку користувачу не показується.", style = MaterialTheme.typography.bodySmall)
    }
}
