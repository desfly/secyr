package ua.homeguard.s3.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.weight
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
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

enum class AddDeviceMethod { ID, IP, NETWORK }

@Composable
fun AddDeviceScreen(
    discoveredDevices: List<DiscoveredDevice>,
    onAddById: (name: String, deviceId: String) -> Unit,
    onAddByIp: (name: String, ip: String) -> Unit,
    onAddDiscovered: (name: String, device: DiscoveredDevice) -> Unit,
    onBack: () -> Unit,
) {
    var method by remember { mutableStateOf(AddDeviceMethod.NETWORK) }
    var name by remember { mutableStateOf("") }
    var value by remember { mutableStateOf("") }

    Column(
        modifier = Modifier.fillMaxSize().padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Text("Додати пристрій", style = MaterialTheme.typography.headlineSmall)
        Text("Назву бачить користувач. Технічний ID у списку не показується.")

        OutlinedTextField(
            value = name,
            onValueChange = { name = it.take(48) },
            label = { Text("Назва пристрою") },
            singleLine = true,
            modifier = Modifier.fillMaxWidth(),
        )

        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            FilterChip(selected = method == AddDeviceMethod.NETWORK, onClick = { method = AddDeviceMethod.NETWORK }, label = { Text("Мережа") })
            FilterChip(selected = method == AddDeviceMethod.IP, onClick = { method = AddDeviceMethod.IP }, label = { Text("IP") })
            FilterChip(selected = method == AddDeviceMethod.ID, onClick = { method = AddDeviceMethod.ID }, label = { Text("ID") })
        }

        when (method) {
            AddDeviceMethod.ID -> {
                OutlinedTextField(
                    value = value,
                    onValueChange = { value = it.trim().take(96) },
                    label = { Text("ID пристрою") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
                Button(
                    enabled = name.isNotBlank() && value.isNotBlank(),
                    onClick = { onAddById(name.trim(), value.trim()) },
                ) { Text("Додати") }
            }

            AddDeviceMethod.IP -> {
                OutlinedTextField(
                    value = value,
                    onValueChange = { value = it.trim().take(128) },
                    label = { Text("IP або адреса") },
                    placeholder = { Text("192.168.1.50") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
                Button(
                    enabled = name.isNotBlank() && value.isNotBlank(),
                    onClick = { onAddByIp(name.trim(), value.trim()) },
                ) { Text("Перевірити й додати") }
            }

            AddDeviceMethod.NETWORK -> {
                Text("Знайдено в локальній мережі: ${discoveredDevices.size}")
                if (discoveredDevices.isEmpty()) {
                    Text("Пошук триває. Переконайтесь, що телефон і HomeGuard-S3 у тій самій мережі.")
                } else {
                    LazyColumn(verticalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.weight(1f, fill = false)) {
                        items(discoveredDevices, key = { it.deviceId }) { device ->
                            Card(modifier = Modifier.fillMaxWidth()) {
                                Column(Modifier.padding(12.dp)) {
                                    Text(device.serviceName.ifBlank { "HomeGuard-S3" }, style = MaterialTheme.typography.titleMedium)
                                    Text("${device.host}:${device.port}")
                                    Spacer(Modifier.height(6.dp))
                                    Button(
                                        enabled = name.isNotBlank(),
                                        onClick = { onAddDiscovered(name.trim(), device) },
                                    ) { Text("Додати як «${name.ifBlank { "…" }}»") }
                                }
                            }
                        }
                    }
                }
            }
        }

        Spacer(Modifier.weight(1f))
        OutlinedButton(onClick = onBack) { Text("Назад") }
    }
}
