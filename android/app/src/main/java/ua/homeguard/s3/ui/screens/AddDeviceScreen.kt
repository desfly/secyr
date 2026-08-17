package ua.homeguard.s3.ui.screens

import androidx.compose.foundation.Image
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import ua.homeguard.s3.R
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
    var foundDeviceName by remember { mutableStateOf("") }
    val cleanName = manualName.trim()
    val progress = scanStatus.progress.coerceIn(0f, 1f)

    LaunchedEffect(devices) {
        if (!manualAddressTouched && devices.isNotEmpty()) manualAddress = devices.first().host
    }

    selectedDiscoveredDevice?.let { selected ->
        AlertDialog(
            onDismissRequest = {
                selectedDiscoveredDevice = null
                foundDeviceName = ""
            },
            title = { Text("Назва пристрою") },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                    Text("Задайте назву, під якою HomeGuard буде показаний у «Мої пристрої».")
                    OutlinedTextField(
                        value = foundDeviceName,
                        onValueChange = { foundDeviceName = it.take(40) },
                        label = { Text("Назва") },
                        singleLine = true,
                        modifier = Modifier.fillMaxWidth(),
                    )
                }
            },
            confirmButton = {
                Button(
                    enabled = foundDeviceName.trim().isNotBlank(),
                    onClick = { onUseDevice(selected, foundDeviceName.trim()) },
                ) { Text("Додати") }
            },
            dismissButton = {
                TextButton(
                    onClick = {
                        selectedDiscoveredDevice = null
                        foundDeviceName = ""
                    },
                ) { Text("Скасувати") }
            },
        )
    }

    Box(modifier = Modifier.fillMaxSize()) {
        Image(
            painter = painterResource(R.drawable.bruce_launcher),
            contentDescription = null,
            contentScale = ContentScale.Crop,
            modifier = Modifier.fillMaxSize().alpha(0.28f),
        )

        Column(
            modifier = Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .padding(horizontal = 16.dp, vertical = 14.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Surface(
                color = MaterialTheme.colorScheme.surface.copy(alpha = 0.88f),
                shape = MaterialTheme.shapes.large,
                tonalElevation = 4.dp,
            ) {
                Column(
                    modifier = Modifier.fillMaxWidth().padding(14.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    Text("Додати пристрій", style = MaterialTheme.typography.headlineSmall)
                    OutlinedButton(onClick = onBack, modifier = Modifier.fillMaxWidth()) { Text("← Назад до пристроїв") }
                }
            }

            Surface(
                color = MaterialTheme.colorScheme.surface.copy(alpha = 0.86f),
                shape = MaterialTheme.shapes.large,
            ) {
                Column(
                    modifier = Modifier.fillMaxWidth().padding(14.dp),
                    verticalArrangement = Arrangement.spacedBy(10.dp),
                ) {
                    Text("Пошук у мережі", style = MaterialTheme.typography.titleLarge)
                    Text(
                        "HomeGuard, який уже підключений до Wi‑Fi, знайдеться автоматично. Телефон і контролер мають бути в одній локальній мережі.",
                        style = MaterialTheme.typography.bodyMedium,
                    )
                    if (isScanning || scanStatus.phase == "done" || scanStatus.phase == "error") {
                        Text(
                            when {
                                isScanning -> "Пошук пристроїв…"
                                scanStatus.phase == "error" -> "Пошук завершився з помилкою"
                                else -> "Пошук завершено"
                            },
                            style = MaterialTheme.typography.titleMedium,
                        )
                        LinearProgressIndicator(progress = { progress }, modifier = Modifier.fillMaxWidth())
                    }
                    if (scanStatus.error.isNotBlank()) {
                        Text(scanStatus.error, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.error)
                    }
                    Button(onClick = onRescan, enabled = !isScanning, modifier = Modifier.fillMaxWidth()) {
                        Text(if (isScanning) "Шукаємо…" else if (devices.isEmpty()) "Почати пошук" else "Шукати знову")
                    }
                }
            }

            if (devices.isEmpty()) {
                Surface(
                    color = MaterialTheme.colorScheme.surface.copy(alpha = 0.82f),
                    shape = MaterialTheme.shapes.large,
                ) {
                    Text(
                        if (isScanning) "Очікуємо відповіді HomeGuard…" else "Знайдених пристроїв поки немає",
                        modifier = Modifier.fillMaxWidth().padding(14.dp),
                        style = MaterialTheme.typography.bodyMedium,
                    )
                }
            } else {
                Text(
                    if (devices.size == 1) "Знайдено: 1 пристрій" else "Знайдено: ${devices.size} пристрої",
                    style = MaterialTheme.typography.titleMedium,
                )
                devices.forEach { device ->
                    Card(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clickable {
                                foundDeviceName = ""
                                selectedDiscoveredDevice = device
                            },
                    ) {
                        Column(
                            modifier = Modifier.fillMaxWidth().padding(14.dp),
                            verticalArrangement = Arrangement.spacedBy(6.dp),
                        ) {
                            Text(device.serviceName.ifBlank { "HomeGuard-S3" }, style = MaterialTheme.typography.titleMedium)
                            Text("Пристрій доступний у локальній мережі", style = MaterialTheme.typography.bodySmall)
                            Text("Торкніться, щоб назвати і додати", color = MaterialTheme.colorScheme.primary, style = MaterialTheme.typography.bodyMedium)
                        }
                    }
                }
            }

            Surface(
                color = MaterialTheme.colorScheme.surface.copy(alpha = 0.84f),
                shape = MaterialTheme.shapes.large,
            ) {
                Column(
                    modifier = Modifier.fillMaxWidth().padding(14.dp),
                    verticalArrangement = Arrangement.spacedBy(10.dp),
                ) {
                    Text("Новий контролер", style = MaterialTheme.typography.titleMedium)
                    Text("Якщо HomeGuard ще не підключений до домашнього Wi‑Fi, виконайте первинне підключення.", style = MaterialTheme.typography.bodyMedium)
                    OutlinedButton(onClick = onProvisioning, modifier = Modifier.fillMaxWidth()) {
                        Text("Підключити HomeGuard до Wi‑Fi")
                    }
                }
            }

            OutlinedButton(onClick = { manualExpanded = !manualExpanded }, modifier = Modifier.fillMaxWidth()) {
                Text(if (manualExpanded) "Сховати ручне додавання" else "Додати вручну")
            }

            if (manualExpanded) {
                Surface(
                    color = MaterialTheme.colorScheme.surface.copy(alpha = 0.9f),
                    shape = MaterialTheme.shapes.large,
                ) {
                    Column(
                        modifier = Modifier.fillMaxWidth().padding(14.dp),
                        verticalArrangement = Arrangement.spacedBy(10.dp),
                    ) {
                        Text("Перед збереженням задайте назву", style = MaterialTheme.typography.titleMedium)
                        OutlinedTextField(
                            value = manualName,
                            onValueChange = { manualName = it.take(40) },
                            label = { Text("Назва пристрою") },
                            supportingText = { Text("Саме ця назва буде показана у списку") },
                            singleLine = true,
                            modifier = Modifier.fillMaxWidth(),
                        )

                        Text("За IP", style = MaterialTheme.typography.titleSmall)
                        OutlinedTextField(
                            value = manualAddress,
                            onValueChange = {
                                manualAddressTouched = true
                                manualAddress = it.trim()
                            },
                            label = { Text("IP або IP:порт") },
                            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Uri),
                            singleLine = true,
                            modifier = Modifier.fillMaxWidth(),
                        )
                        Button(
                            onClick = { onUseManualAddress(cleanName, manualAddress) },
                            enabled = cleanName.isNotBlank() && manualAddress.isNotBlank(),
                            modifier = Modifier.fillMaxWidth(),
                        ) { Text("Додати за IP") }

                        Text("За ID", style = MaterialTheme.typography.titleSmall)
                        OutlinedTextField(
                            value = manualDeviceId,
                            onValueChange = { manualDeviceId = it.trim().take(64) },
                            label = { Text("ID пристрою") },
                            singleLine = true,
                            modifier = Modifier.fillMaxWidth(),
                        )
                        Button(
                            onClick = { onUseDeviceId(cleanName, manualDeviceId) },
                            enabled = cleanName.isNotBlank() && manualDeviceId.isNotBlank(),
                            modifier = Modifier.fillMaxWidth(),
                        ) { Text("Знайти за ID") }
                    }
                }
            }

            Text(
                "У списку пристроїв технічні ID та IP не показуються — тільки вибрана вами назва. Технічні дані доступні у властивостях.",
                style = MaterialTheme.typography.bodySmall,
            )
        }
    }
}
