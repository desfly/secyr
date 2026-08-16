package ua.homeguard.s3.ui.components

import android.content.Intent
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import ua.homeguard.s3.ControllerMaintenanceActivity
import ua.homeguard.s3.diagnostics.SystemDiagnostics

@Composable
fun MaintenancePanel(
    diagnostics: SystemDiagnostics,
    backupStatus: String,
    onExportSettings: () -> Unit,
    onImportSettings: () -> Unit,
) {
    var detailsExpanded by remember { mutableStateOf(false) }
    val connectionProblems = diagnostics.connectionItems.count { !it.ok }
    val hardwareProblems = diagnostics.hardwareItems.count { !it.ok }
    val context = LocalContext.current

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

            OutlinedButton(onClick = onExportSettings, modifier = Modifier.fillMaxWidth()) {
                Text("Backup налаштувань застосунку")
            }
            OutlinedButton(onClick = onImportSettings, modifier = Modifier.fillMaxWidth()) {
                Text("Restore налаштувань застосунку")
            }
            OutlinedButton(
                onClick = { context.startActivity(Intent(context, ControllerMaintenanceActivity::class.java)) },
                modifier = Modifier.fillMaxWidth(),
            ) {
                Text("Конфігурація контролера")
            }
            Text(backupStatus, style = MaterialTheme.typography.bodySmall)
            Text(
                "Backup застосунку не містить API token. Backup контролера, імпорт і Factory Reset доступні окремо після повторного входу Admin.",
                style = MaterialTheme.typography.bodySmall,
            )
        }
    }
}
