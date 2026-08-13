package ua.homeguard.s3.ui.screens

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.combinedClickable
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
import androidx.compose.ui.unit.dp
import ua.homeguard.s3.model.DeviceAccessState
import ua.homeguard.s3.model.RegisteredDevice

@OptIn(ExperimentalFoundationApi::class)
@Composable
fun DeviceListScreen(
    devices: List<RegisteredDevice>,
    onlineDeviceIds: Set<String>,
    onAdd: () -> Unit,
    onQuickView: (RegisteredDevice) -> Unit,
    onOpen: (RegisteredDevice) -> Unit,
    onRename: (RegisteredDevice, String) -> Unit,
) {
    var expandedDeviceId by remember { mutableStateOf<String?>(null) }
    var renameTarget by remember { mutableStateOf<RegisteredDevice?>(null) }
    var renameValue by remember { mutableStateOf("") }

    renameTarget?.let { target ->
        AlertDialog(
            onDismissRequest = { renameTarget = null },
            title = { Text("Назва пристрою") },
            text = {
                OutlinedTextField(
                    value = renameValue,
                    onValueChange = { renameValue = it.take(48) },
                    singleLine = true,
                    label = { Text("Назва") },
                )
            },
            confirmButton = {
                TextButton(
                    enabled = renameValue.isNotBlank(),
                    onClick = {
                        onRename(target, renameValue.trim())
                        renameTarget = null
                    },
                ) { Text("Зберегти") }
            },
            dismissButton = { TextButton(onClick = { renameTarget = null }) { Text("Скасувати") } },
        )
    }

    Column(modifier = Modifier.fillMaxSize().padding(16.dp)) {
        Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Column {
                Text("HomeGuard-S3", style = MaterialTheme.typography.headlineSmall)
                Text("Пристроїв: ${devices.size}", style = MaterialTheme.typography.bodyMedium)
            }
            Button(onClick = onAdd) { Text("Додати") }
        }

        Spacer(Modifier.height(16.dp))
        if (devices.isEmpty()) {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Text("Зареєстрованих пристроїв немає")
                Button(onClick = onAdd) { Text("Додати пристрій") }
            }
            return@Column
        }

        LazyColumn(verticalArrangement = Arrangement.spacedBy(10.dp)) {
            items(devices, key = { it.deviceId }) { device ->
                val revoked = device.accessState == DeviceAccessState.REVOKED
                val online = device.deviceId in onlineDeviceIds
                Card(
                    modifier = Modifier.fillMaxWidth().combinedClickable(
                        onClick = {
                            expandedDeviceId = if (expandedDeviceId == device.deviceId) null else device.deviceId
                            onQuickView(device)
                        },
                        onDoubleClick = { onOpen(device) },
                    )
                ) {
                    Column(Modifier.padding(16.dp)) {
                        Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                            Text(
                                text = device.name,
                                style = MaterialTheme.typography.titleMedium,
                                color = if (revoked) Color.Red else Color.Unspecified,
                            )
                            TextButton(onClick = {
                                renameTarget = device
                                renameValue = device.name
                            }) { Text("Змінити") }
                        }
                        Text(
                            when {
                                revoked -> "Доступ відкликано"
                                online -> "На зв'язку"
                                else -> "Немає зв'язку"
                            },
                            style = MaterialTheme.typography.bodyMedium,
                        )
                        if (expandedDeviceId == device.deviceId) {
                            Spacer(Modifier.height(12.dp))
                            Text("Стан охорони: очікується телеметрія")
                            Text("Аварії: очікується телеметрія")
                            Spacer(Modifier.height(8.dp))
                            OutlinedButton(onClick = { onOpen(device) }) { Text("Повний моніторинг") }
                        }
                    }
                }
            }
        }
    }
}
