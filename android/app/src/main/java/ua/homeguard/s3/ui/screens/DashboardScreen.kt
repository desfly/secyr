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
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.text.style.TextAlign
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
    onFactoryReset: () -> Unit,
    onCommand: (CommandType) -> Unit,
) {
    var pendingDangerousCommand by remember { mutableStateOf<CommandType?>(null) }
    var confirmClearHistory by remember { mutableStateOf(false) }
    var confirmFactoryReset by remember { mutableStateOf(false) }
    var factoryResetPhrase by remember { mutableStateOf("") }
    var eventCategory by remember { mutableStateOf(EventLogCategory.ALL) }
    var eventQuery by remember { mutableStateOf("") }
    var eventSourceText by remember { mutableStateOf("") }
    var pinVisible by remember { mutableStateOf(false) }
    val credentialsReady = operatorId.isNotBlank() && operatorPin.length in 4..12 && operatorPin.all(Char::isDigit)
    val authenticated = accessSession != null
    val adminAuthenticated = accessSession?.role?.name == "ADMIN"
    val canCommand: (CommandType) -> Boolean = { command -> accessSession?.allows(command) == true }
    val sourceFilter = eventSourceText.trim().toIntOrNull()
    val filteredEvents = EventLogFilterEngine.apply(events, EventLogFilter(category = eventCategory, query = eventQuery, sourceId = sourceFilter))

    pendingDangerousCommand?.let { command ->
        AlertDialog(
            onDismissRequest = { pendingDangerousCommand = null },
            title = { Text("Підтвердіть команду") },
            text = { Text("Виконати ${command.name}? Контролер повторно перевірить PIN, роль та challenge.") },
            confirmButton = {
                TextButton(enabled = canCommand(command), onClick = {
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
            confirmButton = { TextButton(onClick = { confirmClearHistory = false; onClearEventHistory() }) { Text("Очистити") } },
            dismissButton = { TextButton(onClick = { confirmClearHistory = false }) { Text("Скасувати") } },
        )
    }
    if (confirmFactoryReset) {
        AlertDialog(
            onDismissRequest = { confirmFactoryReset = false; factoryResetPhrase = "" },
            title = { Text("Повне заводське скидання") },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text("Будуть стерті користувачі, Wi-Fi, Cloud/MQTT і користувацькі налаштування. Апаратна identity та прошивка залишаться.")
                    Text("Для підтвердження введіть ERASE_ALL")
                    OutlinedTextField(
                        value = factoryResetPhrase,
                        onValueChange = { factoryResetPhrase = it.take(9) },
                        modifier = Modifier.fillMaxWidth(),
                        singleLine = true,
                        label = { Text("ERASE_ALL") },
                    )
                }
            },
            confirmButton = {
                TextButton(
                    enabled = adminAuthenticated && factoryResetPhrase == "ERASE_ALL",
                    onClick = {
                        confirmFactoryReset = false
                        factoryResetPhrase = ""
                        onFactoryReset()
                    },
                ) { Text("СТЕРТИ ВСЕ") }
            },
            dismissButton = {
                TextButton(onClick = { confirmFactoryReset = false; factoryResetPhrase = "" }) { Text("Скасувати") }
            },
        )
    }

    LazyColumn(
        modifier = Modifier.padding(horizontal = 16.dp, vertical = 14.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        item {
            Column(modifier = Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                OutlinedButton(onClick = onBackToDevices, modifier = Modifier.fillMaxWidth()) { Text("← Пристрої") }
                Text("HomeGuard-S3", style = MaterialTheme.typography.headlineSmall)
                Text("Версія $versionName", style = MaterialTheme.typography.bodySmall)
                Text("Канал: $route", style = MaterialTheme.typography.bodyMedium)
                Text("Знайдено локально: $localDevices", style = MaterialTheme.typography.bodySmall)
                Button(onClick = onAddDevice, modifier = Modifier.fillMaxWidth()) { Text("+ Додати пристрій") }
            }
        }

        item {
            MaintenancePanel(
                diagnostics = diagnostics,
                backupStatus = backupStatus,
                onExportSettings = onExportSettings,
                onImportSettings = onImportSettings,
            )
        }

        item {
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text("Заводське скидання", style = MaterialTheme.typography.titleMedium)
                    Text("Доступно тільки після входу Admin. Потрібне ручне підтвердження ERASE_ALL.", style = MaterialTheme.typography.bodySmall)
                    OutlinedButton(
                        enabled = adminAuthenticated,
                        onClick = { confirmFactoryReset = true },
                        modifier = Modifier.fillMaxWidth(),
                    ) { Text("Повне заводське скидання") }
                }
            }
        }

        item {
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text("Оператор", style = MaterialTheme.typography.titleMedium)
                    OutlinedTextField(value = operatorId, onValueChange = onOperatorIdChange, modifier = Modifier.fillMaxWidth(), enabled = !authenticated, singleLine = true, label = { Text("ID користувача") })
                    OutlinedTextField(
                        value = operatorPin,
                        onValueChange = { value -> if (value.length <= 12 && value.all(Char::isDigit)) onOperatorPinChange(value) },
                        modifier = Modifier.fillMaxWidth(),
                        enabled = !authenticated,
                        singleLine = true,
                        label = { Text("PIN") },
                        visualTransformation = if (pinVisible) VisualTransformation.None else PasswordVisualTransformation(),
                        trailingIcon = {
                            TextButton(onClick = { pinVisible = !pinVisible }, enabled = !authenticated) {
                                Text(if (pinVisible) "Сховати" else "Показати")
                            }
                        },
                    )
                    if (accessSession == null) {
                        Text(if (credentialsReady) "Готово до входу" else "Введіть ID та PIN 4–12 цифр", style = MaterialTheme.typography.bodySmall)
                        Button(enabled = credentialsReady, onClick = onLogin, modifier = Modifier.fillMaxWidth()) { Text("Увійти") }
                    } else {
                        Text("${accessSession.name} · роль ${accessSession.role.name.lowercase()}")
                        Text(
                            when (accessSession.role.name) {
                                "ADMIN" -> "Повний доступ до керування"
                                "USER" -> "Моніторинг, охорона та клапани"
                                else -> "Тільки моніторинг"
                            },
                            style = MaterialTheme.typography.bodySmall,
                        )
                        OutlinedButton(onClick = onLogout, modifier = Modifier.fillMaxWidth()) { Text("Вийти") }
                    }
                    Text("PIN зберігається тільки в оперативній пам’яті застосунку.", style = MaterialTheme.typography.bodySmall)
                }
            }
        }

        item {
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    Text("Система", style = MaterialTheme.typography.titleMedium)
                    StatusRow("Режим", snapshot.mode.name)
                    StatusRow("Стан", snapshot.health.name)
                    StatusRow("Транспорт", snapshot.transport.name)
                    Text("Телеметрія #${snapshot.sequence}", style = MaterialTheme.typography.bodySmall)
                    Text("Uptime: ${snapshot.uptimeMs} ms", style = MaterialTheme.typography.bodySmall)
                    Text("Команда: $commandStatus", style = MaterialTheme.typography.bodySmall)
                }
            }
        }

        item {
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text("Сповіщення", style = MaterialTheme.typography.titleMedium)
                    NotificationSwitchRow("Критичні", "Тривога, tamper, батарея, offline", criticalNotificationsEnabled, onCheckedChange = onCriticalNotificationsChange)
                    NotificationSwitchRow("Статус системи", "Охорона та зняття з охорони", statusNotificationsEnabled, onCheckedChange = onStatusNotificationsChange)
                    NotificationSwitchRow("Події зон", "Відкриття та закриття зон", zoneNotificationsEnabled, enabled = statusNotificationsEnabled, onCheckedChange = onZoneNotificationsChange)
                }
            }
        }

        item {
            Text("Керування", style = MaterialTheme.typography.titleMedium)
            if (!authenticated) Text("Увійдіть, щоб активувати дозволені команди", style = MaterialTheme.typography.bodySmall)
            Column(modifier = Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(enabled = canCommand(CommandType.ARM_HOME), onClick = { onCommand(CommandType.ARM_HOME) }, modifier = Modifier.fillMaxWidth()) { Text("Охорона: дім") }
                Button(enabled = canCommand(CommandType.ARM_AWAY), onClick = { onCommand(CommandType.ARM_AWAY) }, modifier = Modifier.fillMaxWidth()) { Text("Охорона: повна") }
                OutlinedButton(enabled = canCommand(CommandType.DISARM), onClick = { pendingDangerousCommand = CommandType.DISARM }, modifier = Modifier.fillMaxWidth()) { Text("Зняти з охорони") }
                OutlinedButton(enabled = canCommand(CommandType.SILENCE), onClick = { onCommand(CommandType.SILENCE) }, modifier = Modifier.fillMaxWidth()) { Text("Тиша") }
                OutlinedButton(enabled = canCommand(CommandType.RESET_ALARM), onClick = { pendingDangerousCommand = CommandType.RESET_ALARM }, modifier = Modifier.fillMaxWidth()) { Text("Скинути тривогу") }
                OutlinedButton(enabled = canCommand(CommandType.OPEN_VALVES), onClick = { pendingDangerousCommand = CommandType.OPEN_VALVES }, modifier = Modifier.fillMaxWidth()) { Text("Відкрити клапани") }
                Button(enabled = canCommand(CommandType.CLOSE_VALVES), onClick = { onCommand(CommandType.CLOSE_VALVES) }, modifier = Modifier.fillMaxWidth()) { Text("Закрити клапани") }
            }
        }

        item { Text("Температури", style = MaterialTheme.typography.titleMedium) }
        if (snapshot.temperatures.isEmpty()) item { Text("Немає даних температури") }
        else items(snapshot.temperatures, key = { it.index }) { temperature ->
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.fillMaxWidth().padding(14.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    Text(temperature.name, style = MaterialTheme.typography.titleSmall)
                    Text(temperature.state, style = MaterialTheme.typography.bodySmall)
                    Text(String.format("%.1f °C", temperature.celsius), style = MaterialTheme.typography.titleMedium)
                }
            }
        }

        item { Text("Живлення", style = MaterialTheme.typography.titleMedium) }
        if (snapshot.powerChannels.isEmpty()) item { Text("Немає даних живлення") }
        else items(snapshot.powerChannels, key = { it.index }) { power ->
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.fillMaxWidth().padding(14.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    Text(power.name, style = MaterialTheme.typography.titleSmall)
                    Text(power.state, style = MaterialTheme.typography.bodySmall)
                    Text(String.format("%.2f V · %.2f A · %.1f W", power.voltage, power.current, power.power))
                }
            }
        }

        item {
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text("Журнал подій", style = MaterialTheme.typography.titleMedium)
                    Text("Збережено ${events.size} / 256 · показано ${filteredEvents.size}", style = MaterialTheme.typography.bodySmall)
                    EventCategoryButton("Усі", EventLogCategory.ALL, eventCategory) { eventCategory = it }
                    EventCategoryButton("Критичні", EventLogCategory.CRITICAL, eventCategory) { eventCategory = it }
                    EventCategoryButton("Статус", EventLogCategory.STATUS, eventCategory) { eventCategory = it }
                    EventCategoryButton("Зони", EventLogCategory.ZONES, eventCategory) { eventCategory = it }
                    EventCategoryButton("Інші", EventLogCategory.OTHER, eventCategory) { eventCategory = it }
                    OutlinedTextField(value = eventQuery, onValueChange = { eventQuery = it.take(32) }, modifier = Modifier.fillMaxWidth(), singleLine = true, label = { Text("Пошук") })
                    OutlinedTextField(value = eventSourceText, onValueChange = { value -> if (value.length <= 6 && value.all(Char::isDigit)) eventSourceText = value }, modifier = Modifier.fillMaxWidth(), singleLine = true, label = { Text("Source ID / зона") })
                    OutlinedButton(enabled = events.isNotEmpty(), onClick = onExportEvents, modifier = Modifier.fillMaxWidth()) { Text("Експорт CSV") }
                    OutlinedButton(enabled = events.isNotEmpty(), onClick = onShareEvents, modifier = Modifier.fillMaxWidth()) { Text("Поділитися") }
                    if (eventCategory != EventLogCategory.ALL || eventQuery.isNotBlank() || eventSourceText.isNotBlank()) {
                        OutlinedButton(onClick = { eventCategory = EventLogCategory.ALL; eventQuery = ""; eventSourceText = "" }, modifier = Modifier.fillMaxWidth()) { Text("Скинути фільтри") }
                    }
                    OutlinedButton(enabled = events.isNotEmpty(), onClick = { confirmClearHistory = true }, modifier = Modifier.fillMaxWidth()) { Text("Очистити журнал") }
                }
            }
        }

        if (filteredEvents.isEmpty()) item { Text(if (events.isEmpty()) "Подій ще немає" else "За вибраними фільтрами подій немає") }
        else items(filteredEvents.take(32), key = { it.sequence }) { event ->
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(3.dp)) {
                    Text(event.event, style = MaterialTheme.typography.titleSmall)
                    Text("#${event.sequence} · source ${event.sourceId} · value ${event.value}", style = MaterialTheme.typography.bodySmall)
                    Text("Категорія: ${EventLogFilterEngine.categoryOf(event).name}", style = MaterialTheme.typography.bodySmall)
                }
            }
        }

        item { Text("Зони", style = MaterialTheme.typography.titleMedium) }
        if (snapshot.zones.isEmpty()) item { Text("Очікування живих даних зон…") }
        else items(snapshot.zones, key = { it.index }) { zone ->
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.fillMaxWidth().padding(14.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    Text(zone.name, style = MaterialTheme.typography.titleSmall)
                    Text(if (zone.enabled) "Активна" else "Вимкнена", style = MaterialTheme.typography.bodySmall)
                    Text(zone.state.uppercase())
                }
            }
        }

        item { Text("Тиск / аналогові канали", style = MaterialTheme.typography.titleMedium) }
        if (snapshot.pressures.isEmpty()) item { Text("Немає даних") }
        else items(snapshot.pressures, key = { it.index }) { pressure ->
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.fillMaxWidth().padding(14.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    Text("Канал ${pressure.index + 1}", style = MaterialTheme.typography.titleSmall)
                    Text("${pressure.value}")
                    Text(pressure.state, style = MaterialTheme.typography.bodySmall)
                }
            }
        }
    }
}

@Composable
private fun EventCategoryButton(
    label: String,
    category: EventLogCategory,
    selected: EventLogCategory,
    onSelect: (EventLogCategory) -> Unit,
) {
    if (category == selected) {
        Button(onClick = { onSelect(category) }, modifier = Modifier.fillMaxWidth()) { Text(label) }
    } else {
        OutlinedButton(onClick = { onSelect(category) }, modifier = Modifier.fillMaxWidth()) { Text(label) }
    }
}

@Composable
private fun NotificationSwitchRow(
    label: String,
    detail: String,
    checked: Boolean,
    enabled: Boolean = true,
    onCheckedChange: (Boolean) -> Unit,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        Column(modifier = Modifier.weight(1f)) {
            Text(label, style = MaterialTheme.typography.bodyMedium)
            Text(detail, style = MaterialTheme.typography.bodySmall)
        }
        Switch(checked = checked, enabled = enabled, onCheckedChange = onCheckedChange)
    }
}

@Composable
private fun StatusRow(label: String, value: String) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        Text(label, modifier = Modifier.weight(1f))
        Text(value, modifier = Modifier.weight(1f), textAlign = TextAlign.End)
    }
}
