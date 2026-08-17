package ua.homeguard.s3.ui.screens

import android.content.Context
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.text.KeyboardOptions
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
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.model.SystemSnapshot
import ua.homeguard.s3.network.ControllerIdentity
import ua.homeguard.s3.storage.RegisteredDevice
import ua.homeguard.s3.ui.components.BruceBrand

@OptIn(ExperimentalFoundationApi::class)
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
    val context = LocalContext.current
    val profilePrefs = remember { context.getSharedPreferences("myfist_profile", Context.MODE_PRIVATE) }
    var profileRegistered by remember { mutableStateOf(profilePrefs.getBoolean("registered", false)) }

    if (!profileRegistered) {
        FirstRunRegistrationScreen(
            onRegister = { name, password ->
                profilePrefs.edit()
                    .putBoolean("registered", true)
                    .putString("name", name)
                    .putString("password", password)
                    .apply()
                profileRegistered = true
            },
        )
        return
    }

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
            dismissButton = { TextButton(onClick = { renameDevice = null }) { Text("Скасувати") } },
        )
    }

    deleteDevice?.let { device ->
        AlertDialog(
            onDismissRequest = { deleteDevice = null },
            title = { Text("Видалити пристрій?") },
            text = { Text("«${device.name}» буде видалено зі списку цього телефону. Сам контролер HomeGuard не скидається і не видаляється з мережі.") },
            confirmButton = {
                TextButton(
                    onClick = {
                        onDeleteDevice(device)
                        if (expandedId == device.deviceId) expandedId = null
                        deleteDevice = null
                    },
                ) { Text("Видалити", color = MaterialTheme.colorScheme.error) }
            },
            dismissButton = { TextButton(onClick = { deleteDevice = null }) { Text("Скасувати") } },
        )
    }

    propertiesDevice?.let { device ->
        val discoveredDevice = discovered.firstOrNull { candidate ->
            ControllerIdentity.sameController(device.deviceId, device.baseUrl, candidate.deviceId, candidate.baseUrl)
        }
        val active = device.deviceId.trim().equals(activeDeviceId.trim(), ignoreCase = true)
        AlertDialog(
            onDismissRequest = { propertiesDevice = null },
            title = { Text("Властивості · ${device.name}") },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(7.dp)) {
                    StatusLine("ID", device.deviceId)
                    StatusLine("Адреса", device.baseUrl.ifBlank { "—" })
                    StatusLine("LAN", if (discoveredDevice != null) "доступний" else "не знайдено")
                    StatusLine("Discovery", discoveredDevice?.source?.name ?: "—")
                    StatusLine(
                        "Transport",
                        discoveredDevice?.transport?.name ?: if (active && snapshot.sequence > 0) snapshot.transport.name else "—",
                    )
                    StatusLine("HTTPS", if (discoveredDevice?.secure == true || device.baseUrl.startsWith("https://", true)) "так" else "ні")
                    StatusLine("Авторизація", if (device.authorized) "активна" else "втрачена")
                }
            },
            confirmButton = { TextButton(onClick = { propertiesDevice = null }) { Text("Закрити") } },
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
                Text("Мої пристрої", style = MaterialTheme.typography.headlineSmall)
                Text("Пристрої: ${devices.size}", style = MaterialTheme.typography.titleMedium)
                Text("Стани оновлюються автоматично", style = MaterialTheme.typography.bodySmall)
                Button(onClick = onAddDevice, modifier = Modifier.fillMaxWidth()) { Text("+ Додати пристрій") }
            }
        }

        if (devices.isEmpty()) {
            item {
                Card(modifier = Modifier.fillMaxWidth()) {
                    Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                        Text("Пристроїв ще немає", style = MaterialTheme.typography.titleMedium)
                        Text("Додайте HomeGuard через автоматичний пошук або вручну.")
                        Button(onClick = onAddDevice, modifier = Modifier.fillMaxWidth()) { Text("+ Додати пристрій") }
                    }
                }
            }
        } else {
            items(devices, key = { it.deviceId }) { device ->
                val discoveredOnline = discovered.any { candidate ->
                    ControllerIdentity.sameController(
                        device.deviceId,
                        device.baseUrl,
                        candidate.deviceId,
                        candidate.baseUrl,
                    )
                }
                val active = device.deviceId.trim().equals(activeDeviceId.trim(), ignoreCase = true)
                val online = discoveredOnline || (active && snapshot.sequence > 0)
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
                    Column(modifier = Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(7.dp)) {
                        Text(device.name, style = MaterialTheme.typography.titleMedium, color = titleColor)
                        DeviceStatePicons(
                            online = online,
                            authorized = device.authorized,
                            active = active,
                            snapshot = snapshot,
                        )

                        if (!device.authorized) {
                            Text("Авторизацію втрачено", color = MaterialTheme.colorScheme.error, style = MaterialTheme.typography.bodySmall)
                        }

                        if (expanded) {
                            if (active) {
                                val problemZones = snapshot.zones.filter {
                                    it.state.contains("alarm", true) || it.state.contains("open", true) || it.state.contains("tamper", true) || it.state.contains("fault", true)
                                }
                                val abnormalPressures = snapshot.pressures.count {
                                    !it.state.equals("ok", true) && !it.state.equals("normal", true) && !it.state.equals("unknown", true)
                                }
                                val abnormalTemperatures = snapshot.temperatures.count {
                                    !it.state.equals("ok", true) && !it.state.equals("normal", true) && !it.state.equals("unknown", true)
                                }
                                val abnormalPower = snapshot.powerChannels.count {
                                    !it.state.equals("ok", true) && !it.state.equals("normal", true) && !it.state.equals("unknown", true)
                                }
                                val alarmActive = problemZones.isNotEmpty() || snapshot.health.name.contains("alarm", true) || snapshot.health.name.contains("critical", true)

                                StatusLine("Охорона", snapshot.mode.name)
                                StatusLine("Система", snapshot.health.name)
                                StatusLine("Зв’язок", if (online) snapshot.transport.name else "OFFLINE")
                                StatusLine("Тривога", if (alarmActive) "АКТИВНА" else "немає")
                                StatusLine("Проблемні зони", "${problemZones.size} / ${snapshot.zones.size}")
                                if (snapshot.pressures.isNotEmpty()) {
                                    StatusLine("Тиски", if (abnormalPressures == 0) "норма (${snapshot.pressures.size})" else "проблем: $abnormalPressures")
                                }
                                if (snapshot.temperatures.isNotEmpty()) {
                                    val primary = snapshot.temperatures.first()
                                    StatusLine("Температура", "%.1f °C%s".format(primary.celsius, if (abnormalTemperatures > 0) " · проблем: $abnormalTemperatures" else ""))
                                }
                                if (snapshot.powerChannels.isNotEmpty()) {
                                    val primary = snapshot.powerChannels.first()
                                    StatusLine("Живлення", "%.2f V · %.2f A · %.1f W".format(primary.voltage, primary.current, primary.power))
                                    if (abnormalPower > 0) StatusLine("Живлення стан", "проблем: $abnormalPower")
                                }
                                if (problemZones.isNotEmpty()) {
                                    Text(
                                        "Зони: " + problemZones.take(3).joinToString { it.name } + if (problemZones.size > 3) "…" else "",
                                        color = MaterialTheme.colorScheme.error,
                                        style = MaterialTheme.typography.bodySmall,
                                    )
                                }
                            } else {
                                Text(
                                    if (online) "Контролер доступний у локальній мережі" else "Контролер зараз недоступний",
                                    style = MaterialTheme.typography.bodyMedium,
                                )
                            }

                            OutlinedButton(
                                onClick = {
                                    renameText = device.name
                                    renameDevice = device
                                },
                                modifier = Modifier.fillMaxWidth(),
                            ) { Text("Перейменувати") }
                            OutlinedButton(
                                onClick = { propertiesDevice = device },
                                modifier = Modifier.fillMaxWidth(),
                            ) { Text("Властивості") }
                            Button(onClick = { onOpenDevice(device) }, modifier = Modifier.fillMaxWidth()) { Text("Відкрити") }
                            OutlinedButton(
                                onClick = { deleteDevice = device },
                                modifier = Modifier.fillMaxWidth(),
                            ) { Text("Видалити зі списку", color = MaterialTheme.colorScheme.error) }
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

@Composable
private fun FirstRunRegistrationScreen(
    onRegister: (String, String) -> Unit,
) {
    var name by remember { mutableStateOf("") }
    var password by remember { mutableStateOf("") }
    var passwordVisible by remember { mutableStateOf(false) }
    val cleanName = name.trim()

    Column(
        modifier = Modifier.padding(horizontal = 20.dp, vertical = 28.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        BruceBrand(showTitle = true)
        Text("Реєстрація", style = MaterialTheme.typography.headlineSmall)
        Text("Перший запуск. Створіть локальний профіль користувача.")
        OutlinedTextField(
            value = name,
            onValueChange = { name = it.take(40) },
            label = { Text("Ім’я") },
            singleLine = true,
            modifier = Modifier.fillMaxWidth(),
        )
        OutlinedTextField(
            value = password,
            onValueChange = { password = it.take(64) },
            label = { Text("Пароль") },
            singleLine = true,
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password),
            visualTransformation = if (passwordVisible) VisualTransformation.None else PasswordVisualTransformation(),
            trailingIcon = {
                TextButton(onClick = { passwordVisible = !passwordVisible }) {
                    Text(if (passwordVisible) "🙈" else "👁")
                }
            },
            modifier = Modifier.fillMaxWidth(),
        )
        Button(
            onClick = { onRegister(cleanName, password) },
            enabled = cleanName.isNotBlank() && password.length >= 4,
            modifier = Modifier.fillMaxWidth(),
        ) {
            Text("Зареєструватися")
        }
        Text("Після реєстрації відкриється екран «Мої пристрої».", style = MaterialTheme.typography.bodySmall)
    }
}

@Composable
private fun DeviceStatePicons(
    online: Boolean,
    authorized: Boolean,
    active: Boolean,
    snapshot: SystemSnapshot,
) {
    val problemZones = if (active) snapshot.zones.count {
        it.state.contains("alarm", true) || it.state.contains("open", true) || it.state.contains("tamper", true) || it.state.contains("fault", true)
    } else 0
    val alarm = active && (
        problemZones > 0 || snapshot.health.name.contains("alarm", true) || snapshot.health.name.contains("critical", true)
    )
    val armed = active && snapshot.mode.name.contains("arm", true) && !snapshot.mode.name.contains("disarm", true)

    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Text(
            if (online) "📶 online" else "📵 offline",
            color = if (online) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurfaceVariant,
            style = MaterialTheme.typography.bodyMedium,
        )
        Text(
            when {
                !authorized -> "🔒 доступ"
                armed -> "🛡 охорона"
                active -> "🛡 знято"
                else -> "🛡 —"
            },
            color = if (!authorized) MaterialTheme.colorScheme.error else Color.Unspecified,
            style = MaterialTheme.typography.bodyMedium,
        )
        Text(
            when {
                alarm -> "⚠ тривога"
                active -> "✓ норма"
                else -> "⚠ —"
            },
            color = if (alarm) MaterialTheme.colorScheme.error else Color.Unspecified,
            style = MaterialTheme.typography.bodyMedium,
        )
    }
}

@Composable
private fun StatusLine(label: String, value: String) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        Text(label, modifier = Modifier.weight(1f), style = MaterialTheme.typography.bodyMedium)
        Text(
            value,
            modifier = Modifier.weight(1f),
            textAlign = TextAlign.End,
            style = MaterialTheme.typography.bodyMedium,
        )
    }
}
