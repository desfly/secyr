package ua.homeguard.s3.ui.screens
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import ua.homeguard.s3.model.Diagnostics
@Composable fun DiagnosticsScreen(data:Diagnostics,onBack:()->Unit){Column(Modifier.padding(16.dp),verticalArrangement=Arrangement.spacedBy(8.dp)){Text("Діагностика",style=MaterialTheme.typography.headlineSmall);Text("Канал: ${data.activeTransport}");Text("Помилки: ${data.failedCount}, попередження: ${data.degradedCount}, черга: ${data.queuedCommands}");data.components.forEach{ListItem(headlineContent={Text(it.title)},supportingContent={Text("${it.state} • збоїв ${it.failures}")})};Button(onBack){Text("Назад")}}}
