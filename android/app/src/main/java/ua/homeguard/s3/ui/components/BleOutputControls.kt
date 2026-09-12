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
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

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
