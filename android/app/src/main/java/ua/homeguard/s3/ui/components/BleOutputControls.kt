package ua.homeguard.s3.ui.components

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import org.json.JSONObject
import ua.homeguard.s3.network.ble.BleRuntimeRegistry

private const val ALL_KEYFOB_PERMISSIONS = 0x3f

@Composable
fun BleOutputControlsRuntime() {
    val context = LocalContext.current
    val ble = remember(context) { BleRuntimeRegistry.get(context.applicationContext) }
    val access by ble.access().collectAsState()
    val scope = rememberCoroutineScope()
    var status by remember { mutableStateOf("Готово") }
    var remoteName by remember { mutableStateOf("Брелок 1") }
    var pairingActive by remember { mutableStateOf(false) }

    fun run(label: String, block: suspend () -> JSONObject) {
        scope.launch {
            status = "$label…"
            status = runCatching { block() }.fold(
                onSuccess = { reply ->
                    if (reply.optBoolean("ok", false)) "$label: OK"
                    else "$label: ${reply.optString("reason", reply.optString("status", "відхилено"))}"
                },
                onFailure = { error -> "$label: ${error.message ?: "помилка"}" },
            )
        }
    }

    fun beginKeyfobPairing() {
        val name = remoteName.trim()
        if (name.isEmpty()) {
            status = "Вкажіть назву брелка"
            return
        }
        scope.launch {
            status = "Пошук брелка…"
            val result = runCatching {
                ble.sendCommand(
                    "remote.pair_begin",
                    JSONObject()
                        .put("name", name)
                        .put("permissions", ALL_KEYFOB_PERMISSIONS),
                )
            }
            result.onSuccess { reply ->
                pairingActive = reply.optBoolean("ok", false)
                status = if (pairingActive) {
                    "Натисніть кнопку HomeGuard BLE-брелка протягом 30 с"
                } else {
                    "Pairing: ${reply.optString("reason", "відхилено")}"
                }
            }.onFailure { error ->
                pairingActive = false
                status = "Pairing: ${error.message ?: "помилка"}"
            }
        }
    }

    fun cancelKeyfobPairing() {
        run("Pairing скасовано") {
            ble.sendCommand("remote.pair_cancel")
        }
        pairingActive = false
    }

    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        BleOutputControls(
            enabled = access.authenticated,
            valvesEnabled = access.authenticated && access.valves,
            onLightChange = { active -> run(if (active) "Світло ON" else "Світло OFF") { ble.setLight(active) } },
            onValve1Change = { active -> run(if (active) "Кран 1 ON" else "Кран 1 OFF") { ble.setValve1(active) } },
            onValve2Change = { active -> run(if (active) "Кран 2 ON" else "Кран 2 OFF") { ble.setValve2(active) } },
            onLockPulse = { run("Замок 5 с") { ble.pulseLock() } },
        )

        Card(modifier = Modifier.fillMaxWidth()) {
            Column(
                modifier = Modifier.padding(14.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                Text("BLE · брелки", style = MaterialTheme.typography.titleMedium)
                Text(
                    "Прив’язка дозволена тільки через авторизований BLE-сеанс з правом accessManage.",
                    style = MaterialTheme.typography.bodySmall,
                )
                OutlinedTextField(
                    value = remoteName,
                    onValueChange = { remoteName = it.take(23) },
                    enabled = access.authenticated && access.accessManage && !pairingActive,
                    modifier = Modifier.fillMaxWidth(),
                    singleLine = true,
                    label = { Text("Назва брелка") },
                )
                if (pairingActive) {
                    Button(
                        enabled = access.authenticated && access.accessManage,
                        onClick = ::cancelKeyfobPairing,
                        modifier = Modifier.fillMaxWidth(),
                    ) { Text("Скасувати pairing") }
                } else {
                    Button(
                        enabled = access.authenticated && access.accessManage && remoteName.isNotBlank(),
                        onClick = ::beginKeyfobPairing,
                        modifier = Modifier.fillMaxWidth(),
                    ) { Text("Додати BLE-брелок · 30 с") }
                }
                Text(
                    "Native HomeGuard: Away / Home / Disarm / Lock 5 с / Light / Panic. Захист від повтору — лічильник пакета.",
                    style = MaterialTheme.typography.bodySmall,
                )
            }
        }

        Text(status, style = MaterialTheme.typography.bodySmall)
    }
}

@Composable
fun BleOutputControls(
    enabled: Boolean,
    valvesEnabled: Boolean,
    onLightChange: (Boolean) -> Unit,
    onValve1Change: (Boolean) -> Unit,
    onValve2Change: (Boolean) -> Unit,
    onLockPulse: () -> Unit,
) {
    var confirmLockPulse by remember { mutableStateOf(false) }

    if (confirmLockPulse) {
        AlertDialog(
            onDismissRequest = { confirmLockPulse = false },
            title = { Text("Відкрити замок?") },
            text = { Text("Реле замка буде активоване на 5 секунд і автоматично вимкнеться.") },
            confirmButton = {
                TextButton(
                    enabled = enabled,
                    onClick = {
                        confirmLockPulse = false
                        onLockPulse()
                    },
                ) { Text("Відкрити на 5 с") }
            },
            dismissButton = {
                TextButton(onClick = { confirmLockPulse = false }) { Text("Скасувати") }
            },
        )
    }

    Card(modifier = Modifier.fillMaxWidth()) {
        Column(
            modifier = Modifier.padding(14.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Text("BLE · фізичні виходи", style = MaterialTheme.typography.titleMedium)
            Text(
                "Світло, два крани та замок через авторизований BLE-сеанс.",
                style = MaterialTheme.typography.bodySmall,
            )

            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(enabled = enabled, onClick = { onLightChange(true) }, modifier = Modifier.weight(1f)) {
                    Text("Світло ON")
                }
                OutlinedButton(enabled = enabled, onClick = { onLightChange(false) }, modifier = Modifier.weight(1f)) {
                    Text("Світло OFF")
                }
            }

            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(enabled = valvesEnabled, onClick = { onValve1Change(true) }, modifier = Modifier.weight(1f)) {
                    Text("Кран 1 ON")
                }
                OutlinedButton(enabled = valvesEnabled, onClick = { onValve1Change(false) }, modifier = Modifier.weight(1f)) {
                    Text("Кран 1 OFF")
                }
            }

            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(enabled = valvesEnabled, onClick = { onValve2Change(true) }, modifier = Modifier.weight(1f)) {
                    Text("Кран 2 ON")
                }
                OutlinedButton(enabled = valvesEnabled, onClick = { onValve2Change(false) }, modifier = Modifier.weight(1f)) {
                    Text("Кран 2 OFF")
                }
            }

            Button(
                enabled = enabled,
                onClick = { confirmLockPulse = true },
                modifier = Modifier.fillMaxWidth(),
            ) {
                Text("Замок · 5 секунд")
            }
        }
    }
}
