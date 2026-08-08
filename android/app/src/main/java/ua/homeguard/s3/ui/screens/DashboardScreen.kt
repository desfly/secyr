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
import ua.homeguard.s3.events.EventLogCategory
import ua.homeguard.s3.events.EventLogFilter
import ua.homeguard.s3.events.EventLogFilterEngine
import ua.homeguard.s3.model.CommandType
import ua.homeguard.s3.model.SystemEventRecord
import ua.homeguard.s3.model.SystemSnapshot

@Composable
fun DashboardScreen(
    versionName: String,
    localDevices: Int,
    route: String,
    deviceId: String,
    snapshot: SystemSnapshot,
    events: List<SystemEventRecord>,
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
    onCommand: (CommandType) -> Unit,
) {
    var pendingDangerousCommand by remember { mutableStateOf<CommandType?>(null) }
    var confirmClearHistory by remember { mutableStateOf(false) }
    var eventCategory by remember { mutableStateOf(EventLogCategory.ALL) }
    var eventQuery by remember { mutableStateOf("") }
    var eventSourceText by remember { mutableStateOf("") }
    val credentialsReady = operatorId.isNotBlank() && operatorPin.length in 4..12
    val sourceFilter = eventSourceText.trim().toIntOrNull()
    val filteredEvents = EventLogFilterEngine.apply(
        events,
        EventLogFilter(
            category = eventCategory,
            query = eventQuery,
            sourceId = sourceFilter,
        ),
    )

    pendingDangerousCommand?.let { command ->
        AlertDialog(
            onDismissRequest = { pendingDangerousCommand = null },
            title = { Text("Підтвердіть команду") },
            text = { Text("Виконати ${command.name}? Контролер перевірить PIN оператора та challenge.") },
            confirmButton = {
                TextButton(onClick = {
                    pendingDangerousCommand = null
                    onCommand(command)
                }) { Text("Виконати") }
            },
            dismissButton = { TextButton(onClick = { pendingDangerousCommand = null }) { Text("Скасувати") } },
        )
    }

    if (confirmClearHistory) {
        AlertDialog(
            onDismissRequest = { confirmClearHistory = false },
            title = { Text("Очистити журнал?") },
            text = { Text("Локально збережена історія подій буде видалена з цього телефону. Нові події продовжать записуватися.") },
            confirmButton = {
                TextButton(onClick = {
                    confirmClearHistory = false
                    onClearEventHistory()
                }) { Text("Очистити") }
            },
            dismissButton = { TextButton(onClick = { confirmClearHistory = false }) { Text("Скасувати") } },
        )
    }

    LazyColumn(
        modifier = Modifier.padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        item {
            Text("HomeGuard-S3 $versionName", style = MaterialTheme.typography.headlineMedium)
            Text("Пристрій: $deviceId")
            Text("Канал: $route · знайдено локально: $localDevices")
        }

        item {
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text("Оператор", style = MaterialTheme.typography.titleMedium)
                    OutlinedTextField(
                        value = operatorId,
                        onValueChange = onOperatorIdChange,
                        modifier = Modifier.fillMaxWidth(),
                        singleLine = true,
                        label = { Text("ID користувача") },
                    )
                    OutlinedTextField(
                        value = operatorPin,
                        onValueChange = { value ->
                            if (value.length <= 12 && value.all(Char::isDigit)) onOperatorPinChange(value)
                        },
                        modifier = Modifier.fillMaxWidth(),
                        singleLine = true,
                        label = { Text("PIN") },
                        visualTransformation = PasswordVisualTransformation(),
                    )
                    Text(if (credentialsReady) "PIN готовий до перевірки контролером" else "Введіть ID та PIN 4–12 цифр")
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
                    Text("Критичні сповіщення увімкнені за замовчуванням. Системні налаштування Android каналу мають вищий пріоритет.")
                }
            }
        }

        item {
            Text("Керування", style = MaterialTheme.typography.titleLarge)
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(enabled = credentialsReady, onClick = { onCommand(CommandType.ARM_HOME) }) { Text("Охорона дім") }
                    Button(enabled = credentialsReady, onClick = { onCommand(CommandType.ARM_AWAY) }) { Text("Охорона") }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedButton(enabled = credentialsReady, onClick = { pendingDangerousCommand = CommandType.DISARM }) { Text("Зняти") }
                    OutlinedButton(enabled = credentialsReady, onClick = { onCommand(CommandType.SILENCE) }) { Text("Тиша") }
                    OutlinedButton(enabled = credentialsReady, onClick = { pendingDangerousCommand = CommandType.RESET_ALARM }) { Text("Скинути тривогу") }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedButton(enabled = credentialsReady, onClick = { pendingDangerousCommand = CommandType.OPEN_VALVES }) { Text("Відкрити клапани") }
                    Button(enabled = credentialsReady, onClick = { onCommand(CommandType.CLOSE_VALVES) }) { Text("Закрити клапани") }
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
                    OutlinedTextField(
                        value = eventQuery,
                        onValueChange = { eventQuery = it.take(32) },
                        modifier = Modifier.fillMaxWidth(),
                        singleLine = true,
                        label = { Text("Пошук: тип, значення, sequence") },
                    )
                    OutlinedTextField(
                        value = eventSourceText,
                        onValueChange = { value -> if (value.length <= 6 && value.all(Char::isDigit)) eventSourceText = value },
                        modifier = Modifier.fillMaxWidth(),
                        singleLine = true,
                        label = { Text("Source ID / зона (порожньо = усі)") },
                    )
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        if (eventCategory != EventLogCategory.ALL || eventQuery.isNotBlank() || eventSourceText.isNotBlank()) {
                            OutlinedButton(onClick = {
                                eventCategory = EventLogCategory.ALL
                                eventQuery = ""
                                eventSourceText = ""
                            }) { Text("Скинути фільтри") }
                        }
                        OutlinedButton(enabled = events.isNotEmpty(), onClick = { confirmClearHistory = true }) { Text("Очистити журнал") }
                    }
                    Text("Історія зберігається локально на телефоні та відновлюється після перезапуску застосунку.", style = MaterialTheme.typography.bodySmall)
                }
            }
        }

        if (filteredEvents.isEmpty()) {
            item { Text(if (events.isEmpty()) "Подій ще немає" else "За вибраними фільтрами подій немає") }
        } else {
            items(filteredEvents.take(32), key = { it.sequence }) { event ->
                Card(modifier = Modifier.fillMaxWidth()) {
                    Column(modifier = Modifier.padding(12.dp)) {
                        Text(event.event, style = MaterialTheme.typography.titleSmall)
                        Text("#${event.sequence} · source ${event.sourceId} · value ${event.value}")
                        Text("Категорія: ${EventLogFilterEngine.categoryOf(event).name}", style = MaterialTheme.typography.bodySmall)
                    }
                }
            }
        }

        item { Text("Зони", style = MaterialTheme.typography.titleLarge) }
        if (snapshot.zones.isEmpty()) {
            item { Text("Очікування живих даних зон…") }
        } else {
            items(snapshot.zones, key = { it.index }) { zone ->
                Card(modifier = Modifier.fillMaxWidth()) {
                    Row(modifier = Modifier.fillMaxWidth().padding(14.dp), horizontalArrangement = Arrangement.SpaceBetween) {
                        Column {
                            Text(zone.name, style = MaterialTheme.typography.titleMedium)
                            Text(if (zone.enabled) "Активна" else "Вимкнена")
                        }
                        Text(zone.state.uppercase())
                    }
                }
            }
        }

        item { Text("Тиск / аналогові канали", style = MaterialTheme.typography.titleLarge) }
        if (snapshot.pressures.isEmpty()) {
            item { Text("Немає даних") }
        } else {
            items(snapshot.pressures, key = { it.index }) { pressure ->
                Card(modifier = Modifier.fillMaxWidth()) {
                    Row(modifier = Modifier.fillMaxWidth().padding(14.dp), horizontalArrangement = Arrangement.SpaceBetween) {
                        Text("Канал ${pressure.index + 1}")
                        Text("${pressure.value} · ${pressure.state}")
                    }
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
private fun NotificationSwitchRow(label: String, detail: String, checked: Boolean, enabled: Boolean = true, onCheckedChange: (Boolean) -> Unit) {
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        Column(modifier = Modifier.weight(1f)) {
            Text(label)
            Text(detail, style = MaterialTheme.typography.bodySmall)
        }
        Switch(checked = checked, enabled = enabled, onCheckedChange = onCheckedChange)
    }
}

@Composable
private fun StatusRow(label: String, value: String) {
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        Text(label)
        Text(value)
    }
}
