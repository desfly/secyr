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
import ua.homeguard.s3.diagnostics.SystemDiagnostics

@Composable
fun MaintenancePanel(
    diagnostics: SystemDiagnostics,
    backupStatus: String,
    onExportSettings: () -> Unit,
    onImportSettings: () -> Unit,
    deviceAdminAvailable: Boolean = false,
    onExportDeviceConfig: () -> Unit = {},
    onImportDeviceConfig: () -> Unit = {},
    onFactoryReset: () -> Unit = {},
) {
    var detailsExpanded by remember { mutableStateOf(false) }
    var confirmFactoryReset by remember { mutableStateOf(false) }
    val connectionProblems = diagnostics.connectionItems.count { !it.ok }
    val hardwareProblems = diagnostics.hardwareItems.count { !it.ok }

    if (confirmFactoryReset) {
        AlertDialog(
            onDismissRequest = { confirmFactoryReset = false },
            title = { Text("Повне заводське скидання?") },
            text = {
                Text(
                    "Будуть видалені користувачі, Admin bootstrap state, Wi-Fi, Cloud та інші " +
                        "користувацькі налаштування контролера. Прошивка й апаратна ідентичність залишаться."
                )
            },
            confirmButton = {
                TextButton(
                    enabled = deviceAdminAvailable,
                    onClick = {
                        confirmFactoryReset = false
                        onFactoryReset()
                    },
                ) { Text("СТЕРТИ ВСЕ") }
            },
            dismissButton = {
                TextButton(onClick = { confirmFactoryReset = false }) { Text("Скасувати") }
            },
        )
    }

    Card(modifier = Modifier.fillMaxWidth()) {
        Column(
            modifier = Modifier.padding(horizontal = 14.dp, vertical = 12.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Text("Діагностика", style = MaterialTheme.typography.titleMedium)
            Text(
                if (diagnostics.connectionReady) "Зв’язок: готово"
                else "Зв’язок: проблем $connectionProblems",
                style = MaterialTheme.typography.bodyMedium,
            )
            Text(
                if (diagnostics.hardwareTestReady) "Обладнання: готово"
                else "Обладнання: проблем $hardwareProblems",
                style = MaterialTheme.typography.bodyMedium,
            )

            OutlinedButton(
                onClick = { detailsExpanded = !detailsExpanded },
                modifier = Modifier.fillMaxWidth(),
            ) {
                Text(if (detailsExpanded) "Сховати деталі" else "Показати деталі")
            }

            if (detailsExpanded) {
                diagnostics.connectionItems.forEach { item ->
                    Text(
                        "${if (item.ok) "✓" else "!"} ${item.label}: ${item.detail}",
                        style = MaterialTheme.typography.bodySmall,
                    )
                }
                diagnostics.hardwareItems.forEach { item ->
                    Text(
                        "${if (item.ok) "✓" else "!"} ${item.label}: ${item.detail}",
                        style = MaterialTheme.typography.bodySmall,
                    )
                }
            }

            Text("Застосунок", style = MaterialTheme.typography.titleSmall)
            OutlinedButton(onClick = onExportSettings, modifier = Modifier.fillMaxWidth()) {
                Text("Backup налаштувань застосунку")
            }
            OutlinedButton(onClick = onImportSettings, modifier = Modifier.fillMaxWidth()) {
                Text("Restore налаштувань застосунку")
            }
            Text(
                "API token не входить у backup застосунку і не замінюється під час restore.",
                style = MaterialTheme.typography.bodySmall,
            )

            Text("Контролер", style = MaterialTheme.typography.titleSmall)
            OutlinedButton(
                enabled = deviceAdminAvailable,
                onClick = onExportDeviceConfig,
                modifier = Modifier.fillMaxWidth(),
            ) { Text("Експорт конфігурації контролера") }
            OutlinedButton(
                enabled = deviceAdminAvailable,
                onClick = onImportDeviceConfig,
                modifier = Modifier.fillMaxWidth(),
            ) { Text("Імпорт конфігурації контролера") }
            OutlinedButton(
                enabled = deviceAdminAvailable,
                onClick = { confirmFactoryReset = true },
                modifier = Modifier.fillMaxWidth(),
            ) { Text("Повне заводське скидання") }
            Text(
                if (deviceAdminAvailable) "Операції контролера доступні для активної Admin-сесії."
                else "Увійдіть як Admin через локальне підключення, щоб керувати конфігурацією контролера.",
                style = MaterialTheme.typography.bodySmall,
            )
            Text(backupStatus, style = MaterialTheme.typography.bodySmall)
        }
    }
}
