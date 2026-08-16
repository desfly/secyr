package ua.homeguard.s3.ui.components

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.AlertDialog
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
fun ControllerMaintenanceBar(
    adminAvailable: Boolean,
    status: String,
    onExport: () -> Unit,
    onImport: () -> Unit,
    onFactoryReset: () -> Unit,
) {
    var confirmReset by remember { mutableStateOf(false) }

    if (confirmReset) {
        AlertDialog(
            onDismissRequest = { confirmReset = false },
            title = { Text("Повне заводське скидання?") },
            text = {
                Text(
                    "Контролер видалить усіх користувачів, Wi-Fi, Cloud та інші користувацькі " +
                        "налаштування. Прошивка й апаратна ідентичність залишаться."
                )
            },
            confirmButton = {
                TextButton(
                    enabled = adminAvailable,
                    onClick = {
                        confirmReset = false
                        onFactoryReset()
                    },
                ) { Text("СТЕРТИ ВСЕ") }
            },
            dismissButton = {
                TextButton(onClick = { confirmReset = false }) { Text("Скасувати") }
            },
        )
    }

    Card(modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 6.dp)) {
        Column(
            modifier = Modifier.padding(12.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Text("Конфігурація контролера", style = MaterialTheme.typography.titleMedium)
            OutlinedButton(
                enabled = adminAvailable,
                onClick = onExport,
                modifier = Modifier.fillMaxWidth(),
            ) { Text("Експорт конфігурації") }
            OutlinedButton(
                enabled = adminAvailable,
                onClick = onImport,
                modifier = Modifier.fillMaxWidth(),
            ) { Text("Імпорт конфігурації") }
            OutlinedButton(
                enabled = adminAvailable,
                onClick = { confirmReset = true },
                modifier = Modifier.fillMaxWidth(),
            ) { Text("Повне заводське скидання") }
            Text(
                if (adminAvailable) status else "Увійдіть як Admin через локальне підключення.",
                style = MaterialTheme.typography.bodySmall,
            )
        }
    }
}
