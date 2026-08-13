package ua.homeguard.s3.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import ua.homeguard.s3.model.ExtendedTelemetry
import ua.homeguard.s3.model.SystemSnapshot

@Composable
fun DeviceMonitorScreen(
    deviceName: String,
    snapshot: SystemSnapshot,
    extended: ExtendedTelemetry,
    onBack: () -> Unit,
) {
    LazyColumn(
        modifier = Modifier.fillMaxSize().padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        item {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Column {
                    Text(deviceName, style = MaterialTheme.typography.headlineSmall)
                    Text("Охорона: ${snapshot.mode.name}")
                    Text(
                        if (extended.alarmCount > 0) "Аварії: ${extended.alarmCount}"
                        else "Аварії: немає",
                        color = if (extended.alarmCount > 0) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.onSurface,
                    )
                }
                TextButton(onClick = onBack) { Text("Назад") }
            }
        }

        if (snapshot.zones.isNotEmpty()) {
            item { SectionTitle("Зони") }
            items(snapshot.zones, key = { it.index }) { zone ->
                MonitorCard(zone.name, "Стан: ${zone.state}${if (!zone.enabled) " · вимкнена" else ""}")
            }
        }

        if (extended.outputs.isNotEmpty()) {
            item { SectionTitle("Виходи") }
            items(extended.outputs, key = { it.index }) { output ->
                MonitorCard(
                    output.name,
                    "${if (output.active) "Увімкнено" else "Вимкнено"} · ${output.state}${if (output.controllable) " · керований" else ""}",
                )
            }
        }

        if (extended.temperatures.isNotEmpty()) {
            item { SectionTitle("Температури") }
            items(extended.temperatures, key = { it.index }) { sensor ->
                MonitorCard(sensor.name, "${format1(sensor.valueC)} °C · ${sensor.state}")
            }
        }

        if (snapshot.pressures.isNotEmpty()) {
            item { SectionTitle("Тиски") }
            items(snapshot.pressures, key = { it.index }) { pressure ->
                MonitorCard("Тиск ${pressure.index + 1}", "${format2(pressure.value)} · ${pressure.state}")
            }
        }

        if (extended.electrical.isNotEmpty()) {
            item { SectionTitle("Живлення") }
            items(extended.electrical, key = { it.index }) { channel ->
                val values = buildList {
                    channel.voltageV?.let { add("${format2(it)} V") }
                    channel.currentA?.let { add("${format2(it)} A") }
                    channel.powerW?.let { add("${format1(it)} W") }
                }.joinToString(" · ")
                MonitorCard(channel.name, listOf(values, channel.state).filter { it.isNotBlank() }.joinToString(" · "))
            }
        }

        if (snapshot.zones.isEmpty() && snapshot.pressures.isEmpty() &&
            extended.outputs.isEmpty() && extended.temperatures.isEmpty() && extended.electrical.isEmpty()
        ) {
            item { Text("Очікування телеметрії від пристрою…") }
        }
    }
}

@Composable
private fun SectionTitle(text: String) {
    Text(text, style = MaterialTheme.typography.titleLarge)
}

@Composable
private fun MonitorCard(title: String, value: String) {
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Text(title, style = MaterialTheme.typography.titleMedium)
            Text(value)
        }
    }
}

private fun format1(value: Float): String = String.format(java.util.Locale.US, "%.1f", value)
private fun format2(value: Float): String = String.format(java.util.Locale.US, "%.2f", value)
