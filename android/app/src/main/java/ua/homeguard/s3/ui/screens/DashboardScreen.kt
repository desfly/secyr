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
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import ua.homeguard.s3.model.CommandType
import ua.homeguard.s3.model.SystemSnapshot

@Composable
fun DashboardScreen(
    versionName: String,
    localDevices: Int,
    route: String,
    deviceId: String,
    snapshot: SystemSnapshot,
    commandStatus: String,
    onCommand: (CommandType) -> Unit,
) {
    var pendingDangerousCommand by remember { mutableStateOf<CommandType?>(null) }

    pendingDangerousCommand?.let { command ->
        AlertDialog(
            onDismissRequest = { pendingDangerousCommand = null },
            title = { Text("Підтвердіть команду") },
            text = { Text("Виконати ${command.name}? Для цієї дії контролер також вимагатиме challenge.") },
            confirmButton = {
                TextButton(onClick = {
                    pendingDangerousCommand = null
                    onCommand(command)
                }) { Text("Виконати") }
            },
            dismissButton = { TextButton(onClick = { pendingDangerousCommand = null }) { Text("Скасувати") } },
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
            Text("Керування", style = MaterialTheme.typography.titleLarge)
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(onClick = { onCommand(CommandType.ARM_HOME) }) { Text("Охорона дім") }
                    Button(onClick = { onCommand(CommandType.ARM_AWAY) }) { Text("Охорона") }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedButton(onClick = { pendingDangerousCommand = CommandType.DISARM }) { Text("Зняти") }
                    OutlinedButton(onClick = { onCommand(CommandType.SILENCE) }) { Text("Тиша") }
                    OutlinedButton(onClick = { pendingDangerousCommand = CommandType.RESET_ALARM }) { Text("Скинути тривогу") }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedButton(onClick = { pendingDangerousCommand = CommandType.OPEN_VALVES }) { Text("Відкрити клапани") }
                    Button(onClick = { onCommand(CommandType.CLOSE_VALVES) }) { Text("Закрити клапани") }
                }
            }
        }

        item { Text("Зони", style = MaterialTheme.typography.titleLarge) }
        if (snapshot.zones.isEmpty()) {
            item { Text("Очікування живих даних зон…") }
        } else {
            items(snapshot.zones, key = { it.index }) { zone ->
                Card(modifier = Modifier.fillMaxWidth()) {
                    Row(
                        modifier = Modifier.fillMaxWidth().padding(14.dp),
                        horizontalArrangement = Arrangement.SpaceBetween,
                    ) {
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
                    Row(
                        modifier = Modifier.fillMaxWidth().padding(14.dp),
                        horizontalArrangement = Arrangement.SpaceBetween,
                    ) {
                        Text("Канал ${pressure.index + 1}")
                        Text("${pressure.value} · ${pressure.state}")
                    }
                }
            }
        }
    }
}

@Composable
private fun StatusRow(label: String, value: String) {
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        Text(label)
        Text(value)
    }
}
