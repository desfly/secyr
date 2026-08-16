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
import androidx.compose.runtime.LaunchedEffect
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
    onUseDevice: (DiscoveredDevice, String) -> Unit,
    onUseManualAddress: (String, String) -> Unit,
    onUseDeviceId: (String, String) -> Unit,
    onProvisioning: () -> Unit,
) {
    var manualExpanded by remember { mutableStateOf(false) }
    var manualAddress by remember { mutableStateOf("192.168.4.1") }
    var manualDeviceId by remember { mutableStateOf("") }
    var manualName by remember { mutableStateOf("") }
    var manualAddressTouched by remember { mutableStateOf(false) }
    var selectedDiscoveredDevice by remember { mutableStateOf<DiscoveredDevice?>(null) }
    val cleanName = manualName.trim()
    val progress = scanStatus.progress.coerceIn(0f, 1f)
    val progressPercent = (progress * 100f).toInt()

    LaunchedEffect(devices) {
        if (!manualAddressTouched && devices.isNotEmpty()) {
            manualAddress = devices.first().host
        }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(horizontal = 16.dp, vertical = 14.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        OutlinedButton(onClick = onBack, modifier = Modifier.fillMaxWidth()) { Text("← Назад") }
        Text("Додати пристрій", style = MaterialTheme.typography.headlineSmall)

        Card(modifier = Modifier.fillMaxWidth()) {
            Column(
                modifier = Modifier.padding(horizontal = 14.dp, vertical = 14.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                Text("Новий HomeGuard", style = MaterialTheme.typography.titleMedium)
                Text(
                    "Підключіть контролер до домашньої Wi‑Fi мережі прямо із застосунку.",
                    style = MaterialTheme.typography.bodyMedium,
                )
                Button(onClick = onProvisioning, modifier = Modifier.fillMaxWidth()) {
                    Text("Підключити новий HomeGuard до Wi‑Fi")
                }
            }
        }

        Text("Пошук у локальній мережі", style = MaterialTheme.typography.titleMedium)
        Text(
            "Якщо HomeGuard уже підключений до Wi‑Fi, телефон і контролер мають бути в одній мережі.",
            style = MaterialTheme.typography.bodyMedium,
        )

        Card(modifier = Modifier.fillMaxWidth()) {
            Column(
                modifier = Modifier.padding(horizontal = 14.dp, vertical = 12.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                Text(
                    if (isScanning) "Пошук пристроїв…"
                    else if (scanStatus.phase == "done") "Пошук завершено"
                    else "Готово до пошуку",
                    style = MaterialTheme.typography.titleMedium,
                )
                Text(
                    when (scanStatus.phase) {
                        "sending" -> "Надсилаємо запити"
                        "listening" -> "Слухаємо відповіді"
                        "done" -> "Результати оновлено"
                        "error" -> "Помилка пошуку"
                        else -> "UDP + mDNS"
                    },
                    style = MaterialTheme.typography.bodyMedium,
                )
                Text("Прогрес: $progressPercent%", style = MaterialTheme.typography.titleSmall)
                LinearProgressIndicator(progress = { progress }, modifier = Modifier.fillMaxWidth())

                Text("Надіслано: ${scanStatus.sent}", style = MaterialTheme.typography.bodySmall)
                Text("Отримано: ${scanStatus.received}", style = MaterialTheme.typography.bodySmall)
                Text("Прийнято: ${scanStatus.accepted}", style = MaterialTheme.typography.bodySmall)
                if (scanStatus.network.isNotBlank()) {
                    Text("Мережа: ${scanStatus.network}", style = MaterialTheme.typography.bodySmall)
                }
                if (scanStatus.error.isNotBlank()) {
                    Text(
                        "Діагностика: ${scanStatus.error}",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.error,
                    )
                }

                Button(onClick = onRescan, enabled = !isScanning, modifier = Modifier.fillMaxWidth()) {
                    Text(if (isScanning) "Пошук виконується…" else "Шукати знову")
                }
            }
        }

        Text("Знайдені пристрої", style = MaterialTheme.typography.titleMedium)
        if (devices.isEmpty()) {
            Text(
                if (isScanning) "Очікуємо відповіді HomeGuard…" else "Пристроїв поки не знайдено",
                style = MaterialTheme.typography.bodyMedium,
            )
        } else {
            devices.forEach { device ->
                OutlinedButton(
                    onClick = {
                        selectedDiscoveredDevice = device
                        manualAddress = device.host
                        manualAddressTouched = false
                        manualExpanded = true
                    },
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Column(modifier = Modifier.fillMaxWidth()) {
                        Text(device.serviceName.ifBlank { "HomeGuard" }, style = MaterialTheme.typography.titleMedium)
                        Text(
                            "${device.host}:${device.port} · ${if (device.secure) "HTTPS" else "HTTP"}",
                            style = MaterialTheme.typography.bodySmall,
                        )
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
                supportingText = { Text("Обов’язково задайте власну назву перед збереженням") },
                singleLine = true,
                modifier = Modifier.fillMaxWidth(),
            )
            OutlinedTextField(
                value = manualAddress,
                onValueChange = {
                    manualAddressTouched = true
                    manualAddress = it.trim()
                },
                label = { Text("IP або IP:порт") },
                supportingText = {
                    Text(
                        if (devices.isNotEmpty()) "Автозаповнено з пошуку: ${devices.first().host}"
                        else "За замовчуванням: 192.168.4.1"
                    )
                },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Uri),
                singleLine = true,
                modifier = Modifier.fillMaxWidth(),
            )
            selectedDiscoveredDevice?.let { selected ->
                Button(
                    onClick = { onUseDevice(selected, cleanName) },
                    enabled = cleanName.isNotBlank(),
                    modifier = Modifier.fillMaxWidth(),
                ) { Text("Додати знайдений пристрій") }
                Text(
                    "Знайдено: ${selected.host}:${selected.port}. Перед додаванням задайте власну назву.",
                    style = MaterialTheme.typography.bodySmall,
                )
            }
            Button(
                onClick = { onUseManualAddress(cleanName, manualAddress) },
                enabled = cleanName.isNotBlank() && manualAddress.isNotBlank(),
                modifier = Modifier.fillMaxWidth(),
            ) { Text("Додати за IP") }

            Text("або", style = MaterialTheme.typography.bodyMedium)
            OutlinedTextField(
                value = manualDeviceId,
                onValueChange = { manualDeviceId = it.trim().take(64) },
                label = { Text("ID пристрою") },
                supportingText = { Text("Для пошуку цього HomeGuard у LAN або через Internet/Cloud") },
                singleLine = true,
                modifier = Modifier.fillMaxWidth(),
            )
            Button(
                onClick = { onUseDeviceId(cleanName, manualDeviceId) },
                enabled = cleanName.isNotBlank() && manualDeviceId.isNotBlank(),
                modifier = Modifier.fillMaxWidth(),
            ) { Text("Знайти за ID") }
        }

        Text(
            "Після додавання у списку показується тільки вибрана вами назва пристрою, а не технічний ID чи адреса.",
            style = MaterialTheme.typography.bodySmall,
        )
    }
}
