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
import ua.homeguard.s3.events.EventLogCategory
import ua.homeguard.s3.events.EventLogFilter
import ua.homeguard.s3.events.EventLogFilterEngine
import ua.homeguard.s3.model.CommandType
import ua.homeguard.s3.model.SystemEventRecord
import ua.homeguard.s3.model.SystemSnapshot
import ua.homeguard.s3.ui.components.MaintenancePanel

@Composable
fun DashboardScreen(
    versionName: String,
    localDevices: Int,
    route: String,
    cloudStatus: String,
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
    var eventCategory by rememberSaveable { mutableStateOf(EventLogCategory.ALL) }
    var eventQuery by rememberSaveable { mutableStateOf("") }
    var eventSourceText by rememberSaveable { mutableStateOf("") }
    val listState = rememberSaveable(deviceId, saver = LazyListState.Saver) { LazyListState() }
    val credentialsReady = operatorId.isNotBlank() && operatorPin.length in 4..12
    val sourceFilter = eventSourceText.trim().toIntOrNull()
    val filteredEvents = EventLogFilterEngine.apply(events, EventLogFilter(category = eventCategory, query = eventQuery, sourceId = sourceFilter))

    pendingDangerousCommand?.let { command ->
        AlertDialog(
            onDismissRequest = { pendingDangerousCommand = null },
            title = { Text("Підтвердіть команду") },
            text = { Text("Виконати ${command.name}? Контролер перевірить PIN оператора та challenge.") },
            confirmButton = { TextButton(onClick = { pendingDangerousCommand = null; onCommand(command) }) { Text("Виконати") } },
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

    LazyColumn(
        state = listState,
        modifier = Modifier.padding(horizontal = 10.dp, vertical = 6.dp),
        verticalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        item(key = "header") {
            Text("HomeGuard-S3 $versionName", style = MaterialTheme.typography.titleLarge)
            Text("Пристрій: $deviceId", style = MaterialTheme.typography.bodyMedium)
            Text("Канал: $route · локально: $localDevices", style = MaterialTheme.typography.bodyMedium)
            if (route == "CLOUD") Text("Хмара: $cloudStatus", style = MaterialTheme.typography.bodyMedium)
        }

        item(key = "maintenance") {
            MaintenancePanel(diagnostics, backupStatus, onExportSettings, onImportSettings)
        }

        item(key = "operator") {
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(8.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    Text("Оператор", style = MaterialTheme.typography.titleSmall)
                    OutlinedTextField(operatorId, onOperatorIdChange, modifier = Modifier.fillMaxWidth(), singleLine = true, label = { Text("ID користувача") })
                    OutlinedTextField(
                        operatorPin,
                        { value -> if (value.length <= 12 && value.all(Char::isDigit)) onOperatorPinChange(value) },
                        modifier = Modifier.fillMaxWidth(),
                        singleLine = true,
                        label = { Text("PIN") },
                        visualTransformation = PasswordVisualTransformation(),
                    )
                    Text(if (credentialsReady) "PIN готовий" else "Введіть ID та PIN 4–12 цифр", style = MaterialTheme.typography.bodySmall)
                    Text("PIN лише в оперативній пам’яті.", style = MaterialTheme.typography.bodySmall)
                }
            }
        }

        item(key = "system") {
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(8.dp), verticalArrangement = Arrangement.spacedBy(3.dp)) {
                    Text("Система", style = MaterialTheme.typography.titleSmall)
                    StatusRow("Режим", snapshot.mode.name)
                    StatusRow("Стан", snapshot.health.name)
                    StatusRow("Транспорт", snapshot.transport.name)
                    Text("Телеметрія #${snapshot.sequence} · uptime ${snapshot.uptimeMs} ms", style = MaterialTheme.typography.bodySmall)
                    Text("Команда: $commandStatus", style = MaterialTheme.typography.bodySmall)
                }
            }
        }

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

        item(key = "controls") {
            Text("Керування", style = MaterialTheme.typography.titleMedium)
            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    Button(enabled = credentialsReady, onClick = { onCommand(CommandType.ARM_HOME) }) { Text("Охорона дім") }
                    Button(enabled = credentialsReady, onClick = { onCommand(CommandType.ARM_AWAY) }) { Text("Охорона") }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    OutlinedButton(enabled = credentialsReady, onClick = { pendingDangerousCommand = CommandType.DISARM }) { Text("Зняти") }
                    OutlinedButton(enabled = credentialsReady, onClick = { onCommand(CommandType.SILENCE) }) { Text("Тиша") }
                    OutlinedButton(enabled = credentialsReady, onClick = { pendingDangerousCommand = CommandType.RESET_ALARM }) { Text("Скинути") }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    OutlinedButton(enabled = credentialsReady, onClick = { pendingDangerousCommand = CommandType.OPEN_VALVES }) { Text("Відкрити клапани") }
                    Button(enabled = credentialsReady, onClick = { onCommand(CommandType.CLOSE_VALVES) }) { Text("Закрити клапани") }
                }
            }
        }

        item(key = "event-log-controls") {
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(8.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    Text("Журнал подій", style = MaterialTheme.typography.titleMedium)
                    Text("${events.size}/256 · показано ${filteredEvents.size}", style = MaterialTheme.typography.bodySmall)
                    Row(horizontalArrangement = Arrangement.spacedBy(3.dp)) {
                        EventCategoryButton("Усі", EventLogCategory.ALL, eventCategory) { eventCategory = it }
                        EventCategoryButton("Критичні", EventLogCategory.CRITICAL, eventCategory) { eventCategory = it }
                    }
                    Row(horizontalArrangement = Arrangement.spacedBy(3.dp)) {
                        EventCategoryButton("Статус", EventLogCategory.STATUS, eventCategory) { eventCategory = it }
                        EventCategoryButton("Зони", EventLogCategory.ZONES, eventCategory) { eventCategory = it }
                        EventCategoryButton("Інші", EventLogCategory.OTHER, eventCategory) { eventCategory = it }
                    }
                    OutlinedTextField(eventQuery, { eventQuery = it.take(32) }, modifier = Modifier.fillMaxWidth(), singleLine = true, label = { Text("Пошук") })
                    OutlinedTextField(eventSourceText, { value -> if (value.length <= 6 && value.all(Char::isDigit)) eventSourceText = value }, modifier = Modifier.fillMaxWidth(), singleLine = true, label = { Text("Source ID / зона") })
                    Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                        OutlinedButton(enabled = events.isNotEmpty(), onClick = onExportEvents) { Text("CSV") }
                        OutlinedButton(enabled = events.isNotEmpty(), onClick = onShareEvents) { Text("Поділитися") }
                        OutlinedButton(enabled = events.isNotEmpty(), onClick = { confirmClearHistory = true }) { Text("Очистити") }
                    }
                    if (eventCategory != EventLogCategory.ALL || eventQuery.isNotBlank() || eventSourceText.isNotBlank()) {
                        OutlinedButton(onClick = { eventCategory = EventLogCategory.ALL; eventQuery = ""; eventSourceText = "" }) { Text("Скинути фільтри") }
                    }
                }
            }
        }

        if (filteredEvents.isEmpty()) item(key = "event-log-empty") {
            Text(if (events.isEmpty()) "Подій ще немає" else "За фільтрами подій немає", style = MaterialTheme.typography.bodySmall)
        } else items(filteredEvents.take(32), key = { "event-${it.sequence}" }) { event ->
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(6.dp), verticalArrangement = Arrangement.spacedBy(2.dp)) {
                    Text(event.event, style = MaterialTheme.typography.titleSmall)
                    Text("#${event.sequence} · source ${event.sourceId} · value ${event.value}", style = MaterialTheme.typography.bodySmall)
                    Text(EventLogFilterEngine.categoryOf(event).name, style = MaterialTheme.typography.bodySmall)
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

        item(key = "sensors-title") { Text("Датчики", style = MaterialTheme.typography.titleMedium) }
        if (snapshot.sensors.isEmpty()) item(key = "sensors-empty") { Text("Дані датчиків ще не опубліковані", style = MaterialTheme.typography.bodySmall) }
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

        item(key = "pressures-title") { Text("Тиск / аналогові канали", style = MaterialTheme.typography.titleMedium) }
        if (snapshot.pressures.isEmpty()) item(key = "pressures-empty") { Text("Немає даних", style = MaterialTheme.typography.bodySmall) }
        else items(snapshot.pressures, key = { "pressure-${it.index}" }) { pressure ->
            Card(modifier = Modifier.fillMaxWidth()) {
                Row(modifier = Modifier.fillMaxWidth().padding(7.dp), horizontalArrangement = Arrangement.SpaceBetween) {
                    Text("Канал ${pressure.index + 1}")
                    Text("${pressure.value} · ${pressure.state}")
                }
            }
        }
    }
}

@Composable
private fun EventCategoryButton(label: String, category: EventLogCategory, selected: EventLogCategory, onSelect: (EventLogCategory) -> Unit) {
    if (category == selected) Button(onClick = { onSelect(category) }) { Text(label) }
    else OutlinedButton(onClick = { onSelect(category) }) { Text(label) }
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
