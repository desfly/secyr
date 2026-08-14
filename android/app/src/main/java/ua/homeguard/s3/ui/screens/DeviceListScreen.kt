package ua.homeguard.s3.ui.screens

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.model.SystemSnapshot
import ua.homeguard.s3.storage.RegisteredDevice

@OptIn(ExperimentalFoundationApi::class)
@Composable
fun DeviceListScreen(
    devices: List<RegisteredDevice>,
    discovered: List<DiscoveredDevice>,
    activeDeviceId: String,
    snapshot: SystemSnapshot,
    onAddDevice: () -> Unit,
    onRenameDevice: (RegisteredDevice, String) -> Unit,
    onOpenDevice: (RegisteredDevice) -> Unit,
) {
    var expandedId by remember { mutableStateOf<String?>(null) }
    var renameDevice by remember { mutableStateOf<RegisteredDevice?>(null) }
    var renameText by remember { mutableStateOf("") }
    val onlineIds = discovered.mapTo(hashSetOf()) { it.deviceId }

    renameDevice?.let { device ->
        AlertDialog(
            onDismissRequest = { renameDevice = null },
            title = { Text("Перейменувати пристрій") },
            text = {
                OutlinedTextField(
                    value = renameText,
                    onValueChange = { renameText = it.take(40) },
                    singleLine = true,
                    label = { Text("Назва") },
                )
            },
            confirmButton = {
                TextButton(
                    enabled = renameText.trim().isNotBlank(),
                    onClick = {
                        onRenameDevice(device, renameText.trim())
                        renameDevice = null
                    },
                ) { Text("Зберегти") }
            },
            dismissButton = { TextButton(onClick = { renameDevice = null }) { Text("Скасувати") } },
        )
    }

    LazyColumn(
        modifier = Modifier.padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        item {
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Column {
                    Text("HomeGuard-S3", style = MaterialTheme.typography.headlineMedium)
                    Text("Пристрої: ${devices.size}")
                    Text("Стани оновлюються автоматично", style = MaterialTheme.typography.bodySmall)
                }
                Button(onClick = onAddDevice) { Text("+ Додати") }
            }
        }

        if (devices.isEmpty()) {
            item {
                Card(modifier = Modifier.fillMaxWidth()) {
                    Column(modifier = Modifier.padding(18.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                        Text("Пристроїв ще немає", style = MaterialTheme.typography.titleMedium)
                        Text("Додайте HomeGuard через пошук у мережі або вручну.")
                        Button(onClick = onAddDevice) { Text("+ Додати пристрій") }
                    }
                }
            }
        } else {
            items(devices, key = { it.deviceId }) { device ->
                val online = device.deviceId in onlineIds || (device.deviceId == activeDeviceId && snapshot.sequence > 0)
                val expanded = expandedId == device.deviceId
                val titleColor = if (device.authorized) Color.Unspecified else MaterialTheme.colorScheme.error

                Card(
                    modifier = Modifier
                        .fillMaxWidth()
                        .combinedClickable(
                            onClick = { expandedId = if (expanded) null else device.deviceId },
                            onDoubleClick = { onOpenDevice(device) },
                        )
                ) {
                    Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                        Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                            Text(device.name, style = MaterialTheme.typography.titleLarge, color = titleColor)
                            Text(if (online) "● online" else "○ offline")
                        }

                        if (!device.authorized) Text("Авторизацію втрачено", color = MaterialTheme.colorScheme.error)

                        if (expanded) {
                            if (device.deviceId == activeDeviceId) {
                                Text("Охорона: ${snapshot.mode.name}")
                                Text("Стан: ${snapshot.health.name}")
                                val alarmCount = snapshot.zones.count { it.state.contains("alarm", true) || it.state.contains("open", true) }
                                Text("Активні аварії / зони: $alarmCount")
                            } else {
                                Text(if (online) "Контролер доступний у локальній мережі" else "Контролер зараз недоступний")
                            }
                            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                                OutlinedButton(onClick = {
                                    renameText = device.name
                                    renameDevice = device
                                }) { Text("Перейменувати") }
                                Button(onClick = { onOpenDevice(device) }) { Text("Відкрити") }
                            }
                            Text("Подвійне торкання також відкриває повний моніторинг", style = MaterialTheme.typography.bodySmall)
                        } else {
                            Text("Торкніться для короткого стану · двічі для моніторингу", style = MaterialTheme.typography.bodySmall)
                        }
                    }
                }
            }
        }
    }
}
