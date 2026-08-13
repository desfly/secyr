package ua.homeguard.s3.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import ua.homeguard.s3.model.DiscoveredDevice

@Composable
fun AddDeviceScreen(
    discovered: List<DiscoveredDevice>,
    busy: Boolean,
    message: String,
    onRescan: () -> Unit,
    onAddDiscovered: (device: DiscoveredDevice, displayName: String) -> Unit,
    onAddById: (deviceId: String, displayName: String) -> Unit,
    onAddByIp: (ip: String, displayName: String) -> Unit,
    onBack: () -> Unit,
) {
    var displayName by remember { mutableStateOf("") }
    var deviceId by remember { mutableStateOf("") }
    var ipAddress by remember { mutableStateOf("") }

    Column(
        modifier = Modifier.fillMaxSize().padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text("Додати пристрій", style = MaterialTheme.typography.headlineSmall)
            Button(onClick = onBack, enabled = !busy) { Text("Назад") }
        }

        OutlinedTextField(
            value = displayName,
            onValueChange = { displayName = it.take(64) },
            label = { Text("Назва пристрою") },
            placeholder = { Text("Наприклад: Будинок") },
            singleLine = true,
            modifier = Modifier.fillMaxWidth(),
        )

        Text("Пошук у мережі", style = MaterialTheme.typography.titleMedium)
        Button(onClick = onRescan, enabled = !busy, modifier = Modifier.fillMaxWidth()) {
            Text(if (busy) "Пошук…" else "Знайти HomeGuard у мережі")
        }

        if (discovered.isNotEmpty()) {
            LazyColumn(
                modifier = Modifier.weight(1f, fill = false),
                verticalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                items(discovered, key = { it.deviceId }) { device ->
                    Card(Modifier.fillMaxWidth()) {
                        Column(Modifier.padding(12.dp)) {
                            Text("HomeGuard", style = MaterialTheme.typography.titleMedium)
                            Text(device.host, style = MaterialTheme.typography.bodySmall)
                            Spacer(Modifier.height(8.dp))
                            Button(
                                onClick = { onAddDiscovered(device, displayName) },
                                enabled = !busy && displayName.trim().isNotEmpty(),
                                modifier = Modifier.fillMaxWidth(),
                            ) { Text("Додати як «${displayName.ifBlank { "…" }}»") }
                        }
                    }
                }
            }
        } else {
            Text("У локальній мережі HomeGuard ще не знайдено")
        }

        Text("Або ввести вручну", style = MaterialTheme.typography.titleMedium)
        OutlinedTextField(
            value = deviceId,
            onValueChange = { deviceId = it.take(64) },
            label = { Text("ID пристрою") },
            singleLine = true,
            modifier = Modifier.fillMaxWidth(),
        )
        Button(
            onClick = { onAddById(deviceId.trim(), displayName) },
            enabled = !busy && deviceId.trim().isNotEmpty() && displayName.trim().isNotEmpty(),
            modifier = Modifier.fillMaxWidth(),
        ) { Text("Додати за ID") }

        OutlinedTextField(
            value = ipAddress,
            onValueChange = { ipAddress = it.take(64) },
            label = { Text("IP-адреса") },
            placeholder = { Text("192.168.1.120") },
            singleLine = true,
            modifier = Modifier.fillMaxWidth(),
        )
        Button(
            onClick = { onAddByIp(ipAddress.trim(), displayName) },
            enabled = !busy && ipAddress.trim().isNotEmpty() && displayName.trim().isNotEmpty(),
            modifier = Modifier.fillMaxWidth(),
        ) { Text("Додати за IP") }

        if (message.isNotBlank()) {
            Text(message, color = if (message.startsWith("Помилка")) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.onSurface)
        }
    }
}
