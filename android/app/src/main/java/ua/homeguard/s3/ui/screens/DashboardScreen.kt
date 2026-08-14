package ua.homeguard.s3.ui.screens

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
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import ua.homeguard.s3.diagnostics.SystemDiagnostics
import ua.homeguard.s3.events.EventLogCategory
import ua.homeguard.s3.events.EventLogFilter
import ua.homeguard.s3.events.EventLogFilterEngine
import ua.homeguard.s3.model.AccessSession
import ua.homeguard.s3.model.CommandType
import ua.homeguard.s3.model.SystemEventRecord
import ua.homeguard.s3.model.SystemSnapshot
import ua.homeguard.s3.ui.components.MaintenancePanel

@Composable
fun DashboardScreen(
    versionName: String,
    localDevices: Int,
    route: String,
    deviceId: String,
    snapshot: SystemSnapshot,
    events: List<SystemEventRecord>,
    diagnostics: SystemDiagnostics,
    backupStatus: String,
    commandStatus: String,
    operatorId: String,
    operatorPin: String,
    accessSession: AccessSession?,
    criticalNotificationsEnabled: Boolean,
    statusNotificationsEnabled: Boolean,
    zoneNotificationsEnabled: Boolean,
    onBackToDevices: () -> Unit,
    onAddDevice: () -> Unit,
    onOperatorIdChange: (String) -> Unit,
    onOperatorPinChange: (String) -> Unit,
    onLogin: () -> Unit,
    onLogout: () -> Unit,
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
    var eventCategory by remember { mutableStateOf(EventLogCategory.ALL) }
    var eventQuery by remember { mutableStateOf("") }
    var eventSourceText by remember { mutableStateOf("") }
    val credentialsReady = operatorId.isNotBlank() && operatorPin.length in 4..12 && operatorPin.all(Char::isDigit)
    val authenticated = accessSession != null
    val canCommand: (CommandType) -> Boolean = { command -> accessSession?.allows(command) == true }
    val sourceFilter = eventSourceText.trim().toIntOrNull()
    val filteredEvents = EventLogFilterEngine.apply(events, EventLogFilter(category = eventCategory, query = eventQuery, sourceId = sourceFilter))

    pendingDangerousCommand?.let { command ->
        AlertDialog(
            onDismissRequest = { pendingDangerousCommand = null },
            title = { Text("Підтвердіть команду") },
            text = { Text("Виконати ${command.name}? Контролер повторно перевірить PIN, роль та challenge.") },
            confirmButton = { TextButton(enabled = canCommand(command), onClick = { pendingDangerousCommand = null; onCommand(command) }) { Text("Виконати") } },
            dismissButton = { TextButton(onClick = { pendingDangerousCommand = null }) { Text("Скасувати") } },
        )
    }

    if (confirmClearHistory) {
        AlertDialog(
            onDismissRequest = { confirmClearHistory = false },
            title = { Text("Очистити журнал?") },
            text = { Text("Локально збережена історія подій буде видалена з цього телефону. Нові події продовжать записуватися.") },
            confirmButton = { TextButton(onClick = { confirmClearHistory = false; onClearEventHistory() }) { Text("Очистити") } },
            dismissButton = { TextButton(onClick = { confirmClearHistory = false }) { Text("Скасувати") } },
        )
    }

    LazyColumn(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
        item {
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                OutlinedButton(onClick = onBackToDevices) { Text("← Пристрої") }
                Text("HomeGuard-S3 $versionName", style = MaterialTheme.typography.headlineSmall)
            }
            Text("Канал: $route · знайдено локально: $localDevices")
            Button(onClick = onAddDevice, modifier = Modifier.fillMaxWidth().padding(top = 8.dp)) { Text("+ Додати пристрій") }
        }

        item { MaintenancePanel(diagnostics = diagnostics, backupStatus = backupStatus, onExportSettings = onExportSettings, onImportSettings = onImportSettings) }

        item {
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text("Оператор", style = MaterialTheme.typography.titleMedium)
                    OutlinedTextField(value = operatorId, onValueChange = onOperatorIdChange, modifier = Modifier.fillMaxWidth(), enabled = !authenticated, singleLine = true, label = { Text("ID користувача") })
                    OutlinedTextField(value = operatorPin, onValueChange = { value -> if (value.length <= 12 && value.all(Char::isDigit)) onOperatorPinChange(value) }, modifier = Modifier.fillMaxWidth(), enabled = !authenticated, singleLine = true, label = { Text("PIN") }, visualTransformation = PasswordVisualTransformation())
                    if (accessSession == null) {
                        Text(if (credentialsReady) "Готово до входу" else "Введіть ID та PIN 4–12 цифр")
                        Button(enabled = credentialsReady, onClick = onLogin) { Text("Увійти") }
                    } else {
                        Text("${accessSession.name} · роль ${accessSession.role.name.lowercase()}")
                        Text(when (accessSession.role.name) { "ADMIN" -> "Повний доступ до керування"; "USER" -> "Моніторинг, охорона та клапани"; else -> "Тільки моніторинг" })
                        OutlinedButton(onClick = onLogout) { Text("Вийти") }
                    }
                    Text("PIN зберігається тільки в оперативній пам’яті застосунку.")
                }
            }
        }

        item {
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    Text("Система", style = MaterialTheme.typography.titleMedium)
                    StatusRow("Режим", snapshot.mode.name)
                    StatusRow("Стан", snapshot.health.name)
                    StatusRow("Транспорт", snapshot.transport.name)
                    Text("Телеметрія #${snapshot.sequence} · uptime ${snapshot.uptimeMs} ms")
                    Text("Команда: $commandStatus")
                }
            }
        }

        item {
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text("Сповіщення", style = MaterialTheme.typography.titleMedium)
                    NotificationSwitchRow("Критичні", "Тривога, tamper, батарея, offline", criticalNotificationsEnabled, onCheckedChange = onCriticalNotificationsChange)
                    NotificationSwitchRow("Статус системи", "Охорона та зняття з охорони", statusNotificationsEnabled, onCheckedChange = onStatusNotificationsChange)
                    NotificationSwitchRow("Події зон", "Відкриття та закриття зон", zoneNotificationsEnabled, enabled = statusNotificationsEnabled, onCheckedChange = onZoneNotificationsChange)
                }
            }
        }

        item {
            Text("Керування", style = MaterialTheme.typography.titleLarge)
            if (!authenticated) Text("Увійдіть, щоб активувати дозволені для вашої ролі команди")
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(enabled = canCommand(CommandType.ARM_HOME), onClick = { onCommand(CommandType.ARM_HOME) }) { Text("Охорона дім") }
                    Button(enabled = canCommand(CommandType.ARM_AWAY), onClick = { onCommand(CommandType.ARM_AWAY) }) { Text("Охорона") }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedButton(enabled = canCommand(CommandType.DISARM), onClick = { pendingDangerousCommand = CommandType.DISARM }) { Text("Зняти") }
                    OutlinedButton(enabled = canCommand(CommandType.SILENCE), onClick = { onCommand(CommandType.SILENCE) }) { Text("Тиша") }
                    OutlinedButton(enabled = canCommand(CommandType.RESET_ALARM), onClick = { pendingDangerousCommand = CommandType.RESET_ALARM }) { Text("Скинути тривогу") }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedButton(enabled = canCommand(CommandType.OPEN_VALVES), onClick = { pendingDangerousCommand = CommandType.OPEN_VALVES }) { Text("Відкрити клапани") }
                    Button(enabled = canCommand(CommandType.CLOSE_VALVES), onClick = { onCommand(CommandType.CLOSE_VALVES) }) { Text("Закрити клапани") }
                }
            }
        }

        item {
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text("Журнал подій", style = MaterialTheme.typography.titleLarge)
                    Text("Збережено ${events.size} / 256 · показано ${filteredEvents.size}")
                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        EventCategoryButton("Усі", EventLogCategory.ALL, eventCategory) { eventCategory = it }
                        EventCategoryButton("Критичні", EventLogCategory.CRITICAL, eventCategory) { eventCategory = it }
                    }
                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        EventCategoryButton("Статус", EventLogCategory.STATUS, eventCategory) { eventCategory = it }
                        EventCategoryButton("Зони", EventLogCategory.ZONES, eventCategory) { eventCategory = it }
                        EventCategoryButton("Інші", EventLogCategory.OTHER, eventCategory) { eventCategory = it }
                    }
                    OutlinedTextField(value = eventQuery, onValueChange = { eventQuery = it.take(32) }, modifier = Modifier.fillMaxWidth(), singleLine = true, label = { Text("Пошук: тип, значення, sequence") })
                    OutlinedTextField(value = eventSourceText, onValueChange = { value -> if (value.length <= 6 && value.all(Char::isDigit)) eventSourceText = value }, modifier = Modifier.fillMaxWidth(), singleLine = true, label = { Text("Source ID / зона (порожньо = усі)") })
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        OutlinedButton(enabled = events.isNotEmpty(), onClick = onExportEvents) { Text("Експорт CSV") }
                        OutlinedButton(enabled = events.isNotEmpty(), onClick = onShareEvents) { Text("Поділитися") }
                    }
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        if (eventCategory != EventLogCategory.ALL || eventQuery.isNotBlank() || eventSourceText.isNotBlank()) OutlinedButton(onClick = { eventCategory = EventLogCategory.ALL; eventQuery = ""; eventSourceText = "" }) { Text("Скинути фільтри") }
                        OutlinedButton(enabled = events.isNotEmpty(), onClick = { confirmClearHistory = true }) { Text("Очистити журнал") }
                    }
                }
            }
        }

        if (filteredEvents.isEmpty()) item { Text(if (events.isEmpty()) "Подій ще немає" else "За вибраними фільтрами подій немає") }
        else items(filteredEvents.take(32), key = { it.sequence }) { event ->
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(12.dp)) {
                    Text(event.event, style = MaterialTheme.typography.titleSmall)
                    Text("#${event.sequence} · source ${event.sourceId} · value ${event.value}")
                    Text("Категорія: ${EventLogFilterEngine.categoryOf(event).name}", style = MaterialTheme.typography.bodySmall)
                }
            }
        }

        item { Text("Зони", style = MaterialTheme.typography.titleLarge) }
        if (snapshot.zones.isEmpty()) item { Text("Очікування живих даних зон…") }
        else items(snapshot.zones, key = { it.index }) { zone ->
            Card(modifier = Modifier.fillMaxWidth()) {
                Row(modifier = Modifier.fillMaxWidth().padding(14.dp), horizontalArrangement = Arrangement.SpaceBetween) {
                    Column { Text(zone.name, style = MaterialTheme.typography.titleMedium); Text(if (zone.enabled) "Активна" else "Вимкнена") }
                    Text(zone.state.uppercase())
                }
            }
        }

        item { Text("Тиск / аналогові канали", style = MaterialTheme.typography.titleLarge) }
        if (snapshot.pressures.isEmpty()) item { Text("Немає даних") }
        else items(snapshot.pressures, key = { it.index }) { pressure ->
            Card(modifier = Modifier.fillMaxWidth()) {
                Row(modifier = Modifier.fillMaxWidth().padding(14.dp), horizontalArrangement = Arrangement.SpaceBetween) {
                    Text("Канал ${pressure.index + 1}")
                    Text("${pressure.value} · ${pressure.state}")
                }
            }
        }
    }
}

@Composable
private fun EventCategoryButton(label: String, category: EventLogCategory, selected: EventLogCategory, onSelect: (EventLogCategory) -> Unit) {
    if (category == selected) Button(onClick = { onSelect(category) }) { Text(label) } else OutlinedButton(onClick = { onSelect(category) }) { Text(label) }
}

@Composable
private fun NotificationSwitchRow(label: String, detail: String, checked: Boolean, enabled: Boolean = true, onCheckedChange: (Boolean) -> Unit) {
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        Column(modifier = Modifier.weight(1f)) { Text(label); Text(detail, style = MaterialTheme.typography.bodySmall) }
        Switch(checked = checked, enabled = enabled, onCheckedChange = onCheckedChange)
    }
}

@Composable
private fun StatusRow(label: String, value: String) {
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) { Text(label); Text(value) }
}
