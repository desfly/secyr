package ua.homeguard.s3.ui.components
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import ua.homeguard.s3.model.PressureStatus
@Composable fun PressureCard(value:PressureStatus){Card{ListItem(headlineContent={Text("Тиск ${value.index+1}")},supportingContent={Text("${value.value} бар • ${value.state}")})}}
