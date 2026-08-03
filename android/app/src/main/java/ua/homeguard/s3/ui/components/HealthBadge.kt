package ua.homeguard.s3.ui.components
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import ua.homeguard.s3.model.HealthState
@Composable fun HealthBadge(state:HealthState){Text("Стан: ${state.name}")}
