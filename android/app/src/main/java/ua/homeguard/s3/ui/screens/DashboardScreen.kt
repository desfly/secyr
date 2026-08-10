package ua.homeguard.s3.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListState
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import ua.homeguard.s3.diagnostics.SystemDiagnostics
import ua.homeguard.s3.model.CommandType
import ua.homeguard.s3.model.SystemEventRecord
import ua.homeguard.s3.model.SystemSnapshot
import ua.homeguard.s3.security.CloudAccessRole
import ua.homeguard.s3.security.CloudSessionProfile
import ua.homeguard.s3.ui.components.MaintenancePanel

@Composable
fun DashboardScreen(
    versionName: String,
    localDevices: Int,
    route: String,
    cloudStatus: String,
    cloudProfile: CloudSessionProfile,
    deviceId: String,
    snapshot: SystemSnapshot,
    events: List<SystemEventRecord>,
    diagnostics: SystemDiagnostics,
    backupStatus: String,
    commandStatus: String,
    operatorId: String,
    operatorPin: String,
    criticalNotificationsEnabled: Boolean,
    statusNotificationsEnabled: Boolean,
    zoneNotificationsEnabled: Boolean,
    onOperatorIdChange: (String) -> Unit,
    onOperatorPinChange: (String) -> Unit,
    onLogin: () -> Unit,
    onCriticalNotificationsChange: (Boolean) -> Unit,
    onStatusNotificationsChange: (Boolean) -> Unit,
    onZoneNotificationsChange: (Boolean) -> Unit,
    onClearEventHistory: () -> Unit,
    onExportEvents: () -> Unit,
    onShareEvents: () -> Unit,
    onExportSettings: () -> Unit,
    onImportSettings: () -> Unit,
    onCommand: (CommandType) -> Unit,
) {
    var pendingDangerousCommand by remember { mutableStateOf<CommandType?>(null) }
    var confirmClearHistory by remember { mutableStateOf(false) }
    val listState = rememberSaveable(deviceId, saver = LazyListState.Saver) { LazyListState() }
    val credentialsReady = operatorId.isNotBlank() && operatorPin.length in 4..12
    val cloudMode = route == "CLOUD"
    val authenticated = !cloudMode || cloudProfile.authenticated
    val guest = cloudMode && cloudProfile.role == CloudAccessRole.GUEST
    val admin = !cloudMode || cloudProfile.role == CloudAccessRole.ADMIN
    val showFullState = !cloudMode || cloudProfile.canSubscribeFullState
    val canArm = !cloudMode || cloudProfile.canArm
    val canDisarm = !cloudMode || cloudProfile.canDisarm
    val canValves = !cloudMode || cloudProfile.canControlValves
    val wifiStatus = snapshot.wifiStatus.ifBlank { if (snapshot.transport.name == "WIFI_STA") "ONLINE" else "—" }
    val wifiSsid = snapshot.wifiSsid.ifBlank { "—" }

    pendingDangerousCommand?.let { command ->
        AlertDialog(
            onDismissRequest = { pendingDangerousCommand = null },
            title = { Text("Підтвердіть команду") },
            text = { Text("Виконати ${command.name}? Контролер повторно перевірить PIN і challenge.") },
            confirmButton = { TextButton(onClick = { pendingDangerousCommand = null; onCommand(command) }) { Text("Виконати") } },
            dismissButton = { TextButton(onClick = { pendingDangerousCommand = null }) { Text("Скасувати") } },
        )
    }

    if (confirmClearHistory) {
        AlertDialog(
            onDismissRequest = { confirmClearHistory = false },
            title = { Text("Очистити журнал?") },
            text = { Text("Локальна історія цього телефона буде видалена.") },
            confirmButton = { TextButton(onClick = { confirmClearHistory = false; onClearEventHistory() }) { Text("Очистити") } },
            dismissButton = { TextButton(onClick = { confirmClearHistory = false }) { Text("Скасувати") } },
        )
    }

    LazyColumn(
        state = listState,
        modifier = Modifier.padding(horizontal = 10.dp, vertical = 6.dp),
        verticalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        item(key = "header") {
            Text("HomeGuard-S3 $versionName", style = MaterialTheme.typography.titleLarge)
            Text("Пристрій: $deviceId", style = MaterialTheme.typography.bodyMedium)
            Text("Канал: $route · локально: $localDevices", style = MaterialTheme.typography.bodyMedium)
            if (cloudMode) Text("Хмара: $cloudStatus", style = MaterialTheme.typography.bodyMedium)
        }

        item(key = "operator") {
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(8.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    Text("Мій профіль", style = MaterialTheme.typography.titleSmall)
                    OutlinedTextField(operatorId, onOperatorIdChange, modifier = Modifier.fillMaxWidth(), singleLine = true, label = { Text("ID користувача") })
                    OutlinedTextField(
                        operatorPin,
                        { value -> if (value.length <= 12 && value.all(Char::isDigit)) onOperatorPinChange(value) },
                        modifier = Modifier.fillMaxWidth(),
                        singleLine = true,
                        label = { Text("PIN") },
                        visualTransformation = PasswordVisualTransformation(),
                    )
                    if (cloudMode) {
                        Button(enabled = credentialsReady, onClick = onLogin) { Text("Увійти") }
                        Text(
                            if (cloudProfile.authenticated) "${cloudProfile.name.ifBlank { cloudProfile.id }} · ${cloudProfile.role.name}"
                            else "LOCKED · роль визначає контролер",
                            style = MaterialTheme.typography.bodySmall,
                        )
                        if (cloudProfile.authenticated) {
                            val rights = when (cloudProfile.role) {
                                CloudAccessRole.ADMIN -> "Повний доступ · користувачі · охорона · клапани"
                                CloudAccessRole.USER -> "Моніторинг · охорона/зняття · клапани"
                                CloudAccessRole.GUEST -> "Тільки стан датчиків"
                                CloudAccessRole.LOCKED -> "Немає доступу"
                            }
                            Text("Права: $rights", style = MaterialTheme.typography.bodySmall)
                        }
                    }
                    Text("PIN зберігається тільки в оперативній пам’яті.", style = MaterialTheme.typography.bodySmall)
                    Text("Команда: $commandStatus", style = MaterialTheme.typography.bodySmall)
                }
            }
        }

        if (guest) {
            item(key = "guest-policy") {
                Card(modifier = Modifier.fillMaxWidth()) {
                    Column(modifier = Modifier.padding(8.dp)) {
                        Text("Guest · тільки датчики", style = MaterialTheme.typography.titleSmall)
                        Text("Sensor-only статус оновлюється автоматично кожні 5 секунд. Інформація про охорону, зони, виходи та інших користувачів не завантажується.", style = MaterialTheme.typography.bodySmall)
                    }
                }
            }
        }

        if (showFullState) {
            item(key = "system") {
                Card(modifier = Modifier.fillMaxWidth()) {
                    Column(modifier = Modifier.padding(8.dp), verticalArrangement = Arrangement.spacedBy(3.dp)) {
                        Text("Система", style = MaterialTheme.typography.titleSmall)
                        StatusRow("Режим", snapshot.mode.name)
                        StatusRow("Стан", snapshot.health.name)
                        StatusRow("Транспорт", snapshot.transport.name)
                        StatusRow("Wi‑Fi", wifiStatus)
                        StatusRow("Мережа", wifiSsid)
                        Text("Телеметрія #${snapshot.sequence} · uptime ${snapshot.uptimeMs} ms", style = MaterialTheme.typography.bodySmall)
                    }
                }
            }

            item(key = "controls") {
                Text("Керування", style = MaterialTheme.typography.titleMedium)
                Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    if (canArm) Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                        Button(enabled = credentialsReady && authenticated, onClick = { onCommand(CommandType.ARM_HOME) }) { Text("Охорона дім") }
                        Button(enabled = credentialsReady && authenticated, onClick = { onCommand(CommandType.ARM_AWAY) }) { Text("Охорона") }
                    }
                    if (canDisarm) OutlinedButton(enabled = credentialsReady && authenticated, onClick = { pendingDangerousCommand = CommandType.DISARM }) { Text("Зняти з охорони") }
                    if (canValves) Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                        OutlinedButton(enabled = credentialsReady && authenticated, onClick = { pendingDangerousCommand = CommandType.OPEN_VALVES }) { Text("Відкрити клапани") }
                        Button(enabled = credentialsReady && authenticated, onClick = { onCommand(CommandType.CLOSE_VALVES) }) { Text("Закрити клапани") }
                    }
                }
            }

            item(key = "zones-title") { Text("Зони", style = MaterialTheme.typography.titleMedium) }
            if (snapshot.zones.isEmpty()) item(key = "zones-empty") { Text("Очікування даних зон…", style = MaterialTheme.typography.bodySmall) }
            else items(snapshot.zones, key = { "zone-${it.index}" }) { zone ->
                Card(modifier = Modifier.fillMaxWidth()) {
                    Row(modifier = Modifier.fillMaxWidth().padding(7.dp), horizontalArrangement = Arrangement.SpaceBetween) {
                        Column {
                            Text(zone.name, style = MaterialTheme.typography.titleSmall)
                            Text(if (zone.enabled) "Активна" else "Вимкнена", style = MaterialTheme.typography.bodySmall)
                        }
                        Text(zone.state.uppercase(), style = MaterialTheme.typography.bodyMedium)
                    }
                }
            }
        }

        if (authenticated) {
            item(key = "sensors-title") { Text("Датчики", style = MaterialTheme.typography.titleMedium) }
            if (snapshot.sensors.isEmpty()) item(key = "sensors-empty") { Text(if (guest) "Очікування sensor-only статусу…" else "Дані датчиків ще не опубліковані", style = MaterialTheme.typography.bodySmall) }
            else items(snapshot.sensors, key = { "sensor-${it.index}" }) { sensor ->
                Card(modifier = Modifier.fillMaxWidth()) {
                    Row(modifier = Modifier.fillMaxWidth().padding(7.dp), horizontalArrangement = Arrangement.SpaceBetween) {
                        Column {
                            Text(sensor.name, style = MaterialTheme.typography.titleSmall)
                            if (sensor.value.isNotBlank()) Text(sensor.value, style = MaterialTheme.typography.bodySmall)
                        }
                        Text(sensor.state.uppercase(), style = MaterialTheme.typography.bodyMedium)
                    }
                }
            }
        }

        if (showFullState) {
            item(key = "outputs-title") { Text("Виходи / клапани", style = MaterialTheme.typography.titleMedium) }
            if (snapshot.outputs.isEmpty()) item(key = "outputs-empty") { Text("Немає даних виходів", style = MaterialTheme.typography.bodySmall) }
            else items(snapshot.outputs, key = { "output-${it.index}" }) { output ->
                Card(modifier = Modifier.fillMaxWidth()) {
                    Row(modifier = Modifier.fillMaxWidth().padding(7.dp), horizontalArrangement = Arrangement.SpaceBetween) {
                        Text("Вихід ${output.index}")
                        Text(if (output.active) "ON" else "OFF")
                    }
                }
            }
        }

        if (admin) {
            item(key = "admin-policy") {
                Card(modifier = Modifier.fillMaxWidth()) {
                    Column(modifier = Modifier.padding(8.dp), verticalArrangement = Arrangement.spacedBy(3.dp)) {
                        Text("Admin", style = MaterialTheme.typography.titleSmall)
                        Text("Повний системний доступ. Каталог користувачів доступний тільки ролі Admin; User і Guest бачать тільки власний профіль.", style = MaterialTheme.typography.bodySmall)
                    }
                }
            }
            item(key = "maintenance") { MaintenancePanel(diagnostics, backupStatus, onExportSettings, onImportSettings) }
            item(key = "notifications") {
                Card(modifier = Modifier.fillMaxWidth()) {
                    Column(modifier = Modifier.padding(8.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                        Text("Сповіщення", style = MaterialTheme.typography.titleSmall)
                        NotificationSwitchRow("Критичні", "Тривога, tamper, батарея, offline", criticalNotificationsEnabled, onCriticalNotificationsChange)
                        NotificationSwitchRow("Статус", "Охорона / зняття", statusNotificationsEnabled, onStatusNotificationsChange)
                        NotificationSwitchRow("Зони", "Відкриття / закриття", zoneNotificationsEnabled, onZoneNotificationsChange, enabled = statusNotificationsEnabled)
                    }
                }
            }
            item(key = "event-log") {
                Card(modifier = Modifier.fillMaxWidth()) {
                    Column(modifier = Modifier.padding(8.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                        Text("Журнал подій", style = MaterialTheme.typography.titleMedium)
                        Text("${events.size}/256", style = MaterialTheme.typography.bodySmall)
                        Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                            OutlinedButton(enabled = events.isNotEmpty(), onClick = onExportEvents) { Text("CSV") }
                            OutlinedButton(enabled = events.isNotEmpty(), onClick = onShareEvents) { Text("Поділитися") }
                            OutlinedButton(enabled = events.isNotEmpty(), onClick = { confirmClearHistory = true }) { Text("Очистити") }
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun NotificationSwitchRow(
    label: String,
    detail: String,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
    enabled: Boolean = true,
) {
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        Column(modifier = Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(1.dp)) {
            Text(label, style = MaterialTheme.typography.bodyMedium)
            Text(detail, style = MaterialTheme.typography.bodySmall)
        }
        Switch(checked = checked, enabled = enabled, onCheckedChange = onCheckedChange)
    }
}

@Composable
private fun StatusRow(label: String, value: String) {
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        Text(label, style = MaterialTheme.typography.bodyMedium)
        Text(value, style = MaterialTheme.typography.bodyMedium)
    }
}
