package ua.homeguard.s3.ui.screens
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import ua.homeguard.s3.model.*
import ua.homeguard.s3.ui.UiState
import ua.homeguard.s3.ui.components.*
@Composable fun DashboardScreen(state:UiState,onCommand:(CommandType,Boolean)->Unit,onDiagnostics:()->Unit){Column(Modifier.padding(16.dp),verticalArrangement=Arrangement.spacedBy(8.dp)){ConnectionBanner(state.connected,state.snapshot.transport,state.diagnostics?.queuedCommands?:0);HealthBadge(state.snapshot.health);Text("Режим: ${state.snapshot.mode}");state.snapshot.zones.forEach{ZoneCard(it)};state.snapshot.pressures.forEach{PressureCard(it)};Row{Button({onCommand(CommandType.ARM_AWAY,false)}){Text("Охорона")};Button({onCommand(CommandType.DISARM,false)}){Text("Зняти")}};Button(onDiagnostics){Text("Діагностика")}}}
