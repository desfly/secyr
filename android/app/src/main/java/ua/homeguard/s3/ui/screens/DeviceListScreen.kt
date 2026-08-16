package ua.homeguard.s3.ui.screens

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.Image
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
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
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import ua.homeguard.s3.R
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.model.SystemSnapshot
import ua.homeguard.s3.storage.RegisteredDevice
import ua.homeguard.s3.ui.components.BruceBrand

private val StatusGreen = Color(0xFF00C853)
private val StatusIdle = Color(0xFF8A94A6)

private fun isLegacyGeneratedName(name: String): Boolean {
    val clean = name.trim()
    return clean.isBlank() ||
        clean.equals("HomeGuard", ignoreCase = true) ||
        clean.equals("HomeGuard-S3", ignoreCase = true)
}

private fun visibleDeviceName(name: String): String =
    if (isLegacyGeneratedName(name)) "Без назви" else name.trim()

@OptIn(ExperimentalFoundationApi::class)
@Suppress("UNUSED_PARAMETER")
@Composable
fun DeviceListScreen(
    devices: List<RegisteredDevice>,
    discovered: List<DiscoveredDevice>,
    activeDeviceId: String,
    snapshot: SystemSnapshot,
    onAddDevice: () -> Unit,
    onRenameDevice: (RegisteredDevice, String) -> Unit,
    onDeleteDevice: (RegisteredDevice) -> Unit,
    onOpenDevice: (RegisteredDevice) -> Unit,
) {
    var expandedId by remember { mutableStateOf<String?>(null) }
    var renameDevice by remember { mutableStateOf<RegisteredDevice?>(null) }
    var renameText by remember { mutableStateOf("") }
    var deleteDevice by remember { mutableStateOf<RegisteredDevice?>(null) }
    var propertiesDevice by remember { mutableStateOf<RegisteredDevice?>(null) }

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
                    supportingText = { Text("У списку показується тільки ваша назва") },
                    modifier = Modifier.fillMaxWidth(),
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
            dismissButton = {
                TextButton(onClick = { renameDevice = null }) { Text("Скасувати") }
            },
        )
    }

    deleteDevice?.let { device ->
        AlertDialog(
            onDismissRequest = { deleteDevice = null },
            title = { Text("Видалити пристрій?") },
            text = {
                Text(
                    "«${visibleDeviceName(device.name)}» буде видалено зі списку цього телефону. " +
                        "Сам контролер HomeGuard не скидається і не видаляється з мережі.",
                )
            },
            confirmButton = {
                TextButton(
                    onClick = {
                        onDeleteDevice(device)
                        if (expandedId == device.deviceId) expandedId = null
                        deleteDevice = null
                    },
                ) { Text("Видалити", color = MaterialTheme.colorScheme.error) }
            },
            dismissButton = {
                TextButton(onClick = { deleteDevice = null }) { Text("Скасувати") }
            },
        )
    }

    propertiesDevice?.let { device ->
        val endpoint = device.baseUrl.trim().trimEnd('/').lowercase()
        val found = discovered
            .filter { candidate ->
                candidate.deviceId.equals(device.deviceId, ignoreCase = true) ||
                    (endpoint.isNotBlank() &&
                        candidate.baseUrl.trim().trimEnd('/').lowercase() == endpoint)
            }
            .maxByOrNull { it.seenAtMs }
        val cloudText = when {
            found?.cloudConnected == true -> "підключено"
            found?.cloudConfigured == true -> "налаштовано, немає зв’язку"
            found?.cloudConfigured == false -> "не налаштовано"
            else -> "стан невідомий"
        }
        val visibleName = visibleDeviceName(device.name)

        AlertDialog(
            onDismissRequest = { propertiesDevice = null },
            title = { Text("Властивості · $visibleName") },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    StatusLine("Назва", visibleName)
                    StatusLine("ID", device.deviceId)
                    StatusLine("Адреса", device.baseUrl.ifBlank { "—" })
                    StatusLine("LAN", if (found != null) "доступний" else "недоступний")
                    StatusLine("CLOUD", cloudText)
                    StatusLine("Транспорт", found?.transport?.name ?: "—")
                    StatusLine("Виявлення", found?.source?.name ?: "—")
                    StatusLine("HTTPS", found?.secure?.let { if (it) "так" else "ні" } ?: "—")
                    StatusLine("Авторизація", if (device.authorized) "активна" else "втрачена")
                }
            },
            confirmButton = {
                TextButton(onClick = { propertiesDevice = null }) { Text("Закрити") }
            },
        )
    }

    LazyColumn(
        modifier = Modifier.padding(horizontal = 16.dp, vertical = 14.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        item {
            Column(
                modifier = Modifier.fillMaxWidth(),
                verticalArrangement = Arrangement.spacedBy(6.dp),
            ) {
                BruceBrand(showTitle = true)
                Text("Пристрої: ${devices.size}", style = MaterialTheme.typography.titleMedium)
                Text(
                    "У списку показуються тільки ваші назви та короткий стан зв’язку",
                    style = MaterialTheme.typography.bodySmall,
                )
            }
        }

        if (devices.isEmpty()) {
            item {
                Card(modifier = Modifier.fillMaxWidth()) {
                    Column(
                        modifier = Modifier.padding(16.dp),
                        verticalArrangement = Arrangement.spacedBy(8.dp),
                    ) {
                        Text("Пристроїв ще немає", style = MaterialTheme.typography.titleMedium)
                        Text(
                            "Додавання пристроїв тимчасово сховане. " +
                                "Застосунок не повинен автоматично відкривати старий Add Device flow.",
                        )
                    }
                }
            }
        } else {
            items(devices, key = { it.deviceId }) { device ->
                val endpoint = device.baseUrl.trim().trimEnd('/').lowercase()
                val found = discovered
                    .filter { candidate ->
                        candidate.deviceId.equals(device.deviceId, ignoreCase = true) ||
                            (endpoint.isNotBlank() &&
                                candidate.baseUrl.trim().trimEnd('/').lowercase() == endpoint)
                    }
                    .maxByOrNull { it.seenAtMs }
                val lanOnline = found != null
                val cloudOnline = found?.cloudConnected == true
                val online = lanOnline || cloudOnline
                val expanded = expandedId == device.deviceId
                val legacyName = isLegacyGeneratedName(device.name)
                val visibleName = visibleDeviceName(device.name)
                val titleColor =
                    if (device.authorized && !legacyName) Color.Unspecified else MaterialTheme.colorScheme.error

                Card(
                    modifier = Modifier
                        .fillMaxWidth()
                        .combinedClickable(
                            onClick = { expandedId = if (expanded) null else device.deviceId },
                            onDoubleClick = { onOpenDevice(device) },
                        ),
                ) {
                    Column(
                        modifier = Modifier.padding(14.dp),
                        verticalArrangement = Arrangement.spacedBy(9.dp),
                    ) {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.spacedBy(10.dp),
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            Image(
                                painter = painterResource(R.drawable.ic_homeguard_device),
                                contentDescription = "HomeGuard",
                                modifier = Modifier.size(36.dp),
                            )
                            Column(
                                modifier = Modifier.weight(1f),
                                verticalArrangement = Arrangement.spacedBy(3.dp),
                            ) {
                                Text(
                                    visibleName,
                                    style = MaterialTheme.typography.titleMedium,
                                    color = titleColor,
                                )
                                Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                                    Text(
                                        if (lanOnline) "● LAN" else "○ LAN",
                                        style = MaterialTheme.typography.bodySmall,
                                        color = if (lanOnline) StatusGreen else StatusIdle,
                                    )
                                    Text(
                                        "☁ CLOUD",
                                        style = MaterialTheme.typography.bodySmall,
                                        color = if (cloudOnline) StatusGreen else StatusIdle,
                                    )
                                    Text(
                                        if (online) "● online" else "○ offline",
                                        style = MaterialTheme.typography.bodySmall,
                                        color = if (online) StatusGreen else StatusIdle,
                                    )
                                }
                            }
                        }

                        if (legacyName) {
                            Text(
                                "Потрібно задати вашу назву пристрою",
                                color = MaterialTheme.colorScheme.error,
                                style = MaterialTheme.typography.bodySmall,
                            )
                        }

                        if (!device.authorized) {
                            Text(
                                "Авторизацію втрачено",
                                color = MaterialTheme.colorScheme.error,
                                style = MaterialTheme.typography.bodySmall,
                            )
                        }

                        if (expanded) {
                            if (device.deviceId == activeDeviceId) {
                                val problemZones = snapshot.zones.filter {
                                    it.state.contains("alarm", true) ||
                                        it.state.contains("open", true) ||
                                        it.state.contains("tamper", true) ||
                                        it.state.contains("fault", true)
                                }
                                val abnormalPressures = snapshot.pressures.count {
                                    !it.state.equals("ok", true) &&
                                        !it.state.equals("normal", true) &&
                                        !it.state.equals("unknown", true)
                                }
                                val abnormalTemperatures = snapshot.temperatures.count {
                                    !it.state.equals("ok", true) &&
                                        !it.state.equals("normal", true) &&
                                        !it.state.equals("unknown", true)
                                }
                                val abnormalPower = snapshot.powerChannels.count {
                                    !it.state.equals("ok", true) &&
                                        !it.state.equals("normal", true) &&
                                        !it.state.equals("unknown", true)
                                }
                                val alarmActive = problemZones.isNotEmpty() ||
                                    snapshot.health.name.contains("alarm", true) ||
                                    snapshot.health.name.contains("critical", true)

                                StatusLine("Охорона", snapshot.mode.name)
                                StatusLine("Система", snapshot.health.name)
                                StatusLine("Тривога", if (alarmActive) "АКТИВНА" else "немає")
                                StatusLine("Проблемні зони", "${problemZones.size} / ${snapshot.zones.size}")
                                if (snapshot.pressures.isNotEmpty()) {
                                    StatusLine(
                                        "Тиски",
                                        if (abnormalPressures == 0) {
                                            "норма (${snapshot.pressures.size})"
                                        } else {
                                            "проблем: $abnormalPressures"
                                        },
                                    )
                                }
                                if (snapshot.temperatures.isNotEmpty()) {
                                    val primary = snapshot.temperatures.first()
                                    StatusLine(
                                        "Температура",
                                        "%.1f °C%s".format(
                                            primary.celsius,
                                            if (abnormalTemperatures > 0) {
                                                " · проблем: $abnormalTemperatures"
                                            } else {
                                                ""
                                            },
                                        ),
                                    )
                                }
                                if (snapshot.powerChannels.isNotEmpty()) {
                                    val primary = snapshot.powerChannels.first()
                                    StatusLine(
                                        "Живлення",
                                        "%.2f V · %.2f A · %.1f W".format(
                                            primary.voltage,
                                            primary.current,
                                            primary.power,
                                        ),
                                    )
                                    if (abnormalPower > 0) {
                                        StatusLine("Живлення стан", "проблем: $abnormalPower")
                                    }
                                }
                                if (problemZones.isNotEmpty()) {
                                    Text(
                                        "Зони: " + problemZones.take(3).joinToString { it.name } +
                                            if (problemZones.size > 3) "…" else "",
                                        color = MaterialTheme.colorScheme.error,
                                        style = MaterialTheme.typography.bodySmall,
                                    )
                                }
                            } else {
                                Text(
                                    if (online) "Контролер доступний" else "Контролер зараз недоступний",
                                    style = MaterialTheme.typography.bodyMedium,
                                )
                            }

                            OutlinedButton(
                                onClick = {
                                    renameText = if (legacyName) "" else device.name
                                    renameDevice = device
                                },
                                modifier = Modifier.fillMaxWidth(),
                            ) { Text("Перейменувати") }

                            OutlinedButton(
                                onClick = { propertiesDevice = device },
                                modifier = Modifier.fillMaxWidth(),
                            ) { Text("Властивості") }

                            Button(
                                onClick = { onOpenDevice(device) },
                                modifier = Modifier.fillMaxWidth(),
                            ) { Text("Відкрити") }

                            OutlinedButton(
                                onClick = { deleteDevice = device },
                                modifier = Modifier.fillMaxWidth(),
                            ) {
                                Text(
                                    "Видалити зі списку",
                                    color = MaterialTheme.colorScheme.error,
                                )
                            }

                            Text(
                                "Подвійне торкання також відкриває повний моніторинг",
                                style = MaterialTheme.typography.bodySmall,
                            )
                        } else {
                            Text(
                                "Торкніться для короткого стану · двічі для моніторингу",
                                style = MaterialTheme.typography.bodySmall,
                            )
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun StatusLine(label: String, value: String) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        Text(
            label,
            modifier = Modifier.weight(1f),
            style = MaterialTheme.typography.bodyMedium,
        )
        Text(
            value,
            modifier = Modifier.weight(1f),
            textAlign = TextAlign.End,
            style = MaterialTheme.typography.bodyMedium,
        )
    }
}
