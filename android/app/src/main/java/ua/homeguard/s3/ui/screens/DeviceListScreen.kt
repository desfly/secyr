package ua.homeguard.s3.ui.screens

import androidx.activity.compose.LocalActivity
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
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import ua.homeguard.s3.BuildConfig
import ua.homeguard.s3.model.DeviceAccessState
import ua.homeguard.s3.model.RegisteredDevice
import ua.homeguard.s3.model.SystemSnapshot
import ua.homeguard.s3.network.HttpDeviceApi
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

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
    val activity = LocalActivity.current
    var expandedDeviceId by remember { mutableStateOf<String?>(null) }
    var renameTarget by remember { mutableStateOf<RegisteredDevice?>(null) }
    var renameValue by remember { mutableStateOf("") }
    val buildDate = remember {
        SimpleDateFormat("dd.MM.yyyy HH:mm", Locale.getDefault()).format(Date(BuildConfig.BUILD_TIME))
    }

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

    Column(
        modifier = Modifier
            .fillMaxSize()
            .safeDrawingPadding()
            .padding(horizontal = 16.dp, vertical = 10.dp),
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            Column(modifier = Modifier.padding(top = 6.dp)) {
                Text("MyFist", style = MaterialTheme.typography.titleLarge)
                Text("Версія: ${BuildConfig.VERSION_NAME}", style = MaterialTheme.typography.bodySmall)
                Text("Збірка: $buildDate", style = MaterialTheme.typography.bodySmall)
                Text("Пристроїв: ${devices.size}", style = MaterialTheme.typography.bodySmall)
            }
            Column {
                TextButton(onClick = onAdd) { Text("+ Додати") }
                TextButton(onClick = { activity?.finish() }) { Text("Вийти") }
            }
        }

        Spacer(Modifier.height(10.dp))
        if (devices.isEmpty()) {
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(
                    modifier = Modifier.padding(16.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    Text("Пристроїв поки немає", style = MaterialTheme.typography.titleMedium)
                    Text(
                        "Додайте HomeGuard-S3 вручну або знайдіть його в локальній мережі.",
                        style = MaterialTheme.typography.bodyMedium,
                    )
                }
            }
            return@Column
        }

        LazyColumn(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            items(devices, key = { it.deviceId }) { device ->
                val revoked = device.accessState == DeviceAccessState.REVOKED
                val online = device.deviceId in onlineDeviceIds
                var quickSnapshot by remember(device.deviceId) { mutableStateOf<SystemSnapshot?>(null) }
                var quickError by remember(device.deviceId) { mutableStateOf<String?>(null) }
                val expanded = expandedDeviceId == device.deviceId

                if (expanded) {
                    LaunchedEffect(device.deviceId, device.lastKnownUrl, online, revoked) {
                        quickSnapshot = null
                        quickError = null
                        if (online && !revoked && device.lastKnownUrl.isNotBlank()) {
                            runCatching {
                                HttpDeviceApi(device.lastKnownUrl, tokenProvider = { "" }).snapshot()
                            }.onSuccess { quickSnapshot = it }
                                .onFailure { quickError = it.message ?: "status unavailable" }
                        }
                    }
                }

                Card(
                    modifier = Modifier
                        .fillMaxWidth()
                        .combinedClickable(
                            onClick = {
                                expandedDeviceId = if (expanded) null else device.deviceId
                                onQuickView(device)
                            },
                            onDoubleClick = { if (!revoked) onOpen(device) },
                        ),
                ) {
                    Column(Modifier.padding(horizontal = 14.dp, vertical = 10.dp)) {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween,
                        ) {
                            Text(
                                text = device.name,
                                style = MaterialTheme.typography.titleMedium,
                                color = if (revoked) MaterialTheme.colorScheme.error else Color.Unspecified,
                                modifier = Modifier.padding(top = 8.dp),
                            )
                            TextButton(onClick = {
                                renameTarget = device
                                renameValue = device.name
                            }) { Text("Назва") }
                        }
                        Text(
                            when {
                                revoked -> "Доступ відкликано"
                                online -> "На зв'язку"
                                else -> "Немає зв'язку"
                            },
                            style = MaterialTheme.typography.bodySmall,
                            color = if (revoked) MaterialTheme.colorScheme.error else Color.Unspecified,
                        )
                        if (expanded) {
                            Spacer(Modifier.height(8.dp))
                            Text("Зв'язок: ${if (online) "онлайн" else "офлайн"}", style = MaterialTheme.typography.bodyMedium)
                            Text(
                                "Доступ: ${if (revoked) "відкликано адміністратором" else "активний"}",
                                style = MaterialTheme.typography.bodyMedium,
                                color = if (revoked) MaterialTheme.colorScheme.error else Color.Unspecified,
                            )
                            when {
                                revoked || !online -> Text("Охорона / аварії: недоступно без зв'язку", style = MaterialTheme.typography.bodyMedium)
                                quickSnapshot != null -> {
                                    Text("Охорона: ${quickSnapshot!!.mode.name}", style = MaterialTheme.typography.bodyMedium)
                                    Text("Стан / аварії: ${quickSnapshot!!.health.name}", style = MaterialTheme.typography.bodyMedium)
                                }
                                quickError != null -> Text("Охорона / аварії: помилка ${quickError}", style = MaterialTheme.typography.bodyMedium)
                                else -> Text("Охорона / аварії: завантаження…", style = MaterialTheme.typography.bodyMedium)
                            }
                            if (!revoked) {
                                Spacer(Modifier.height(6.dp))
                                OutlinedButton(onClick = { onOpen(device) }) { Text("Повний моніторинг") }
                            }
                        }
                    }
                }
            }
        }
    }
}
