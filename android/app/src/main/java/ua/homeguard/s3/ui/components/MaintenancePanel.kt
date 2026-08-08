package ua.homeguard.s3.ui.components

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import ua.homeguard.s3.diagnostics.SystemDiagnostics

@Composable
fun MaintenancePanel(
    diagnostics: SystemDiagnostics,
    backupStatus: String,
    onExportSettings: () -> Unit,
    onImportSettings: () -> Unit,
) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Діагностика та обслуговування", style = MaterialTheme.typography.titleLarge)
            Text(if (diagnostics.connectionReady) "Зв'язок: ГОТОВО" else "Зв'язок: потребує перевірки")
            diagnostics.connectionItems.forEach { item ->
                Text("${if (item.ok) "✓" else "!"} ${item.label}: ${item.detail}")
            }
            Text(if (diagnostics.hardwareTestReady) "Hardware-test: ГОТОВО" else "Hardware-test: ще не готово")
            diagnostics.hardwareItems.forEach { item ->
                Text("${if (item.ok) "✓" else "!"} ${item.label}: ${item.detail}")
            }
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedButton(onClick = onExportSettings) { Text("Backup налаштувань") }
                OutlinedButton(onClick = onImportSettings) { Text("Restore") }
            }
            Text(backupStatus, style = MaterialTheme.typography.bodySmall)
            Text("API token не включається у backup і не замінюється під час restore.", style = MaterialTheme.typography.bodySmall)
        }
    }
}
