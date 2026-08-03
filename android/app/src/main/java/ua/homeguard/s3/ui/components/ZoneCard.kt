package ua.homeguard.s3.ui.components
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import ua.homeguard.s3.model.ZoneStatus
@Composable fun ZoneCard(zone:ZoneStatus){Card{ListItem(headlineContent={Text(zone.name)},supportingContent={Text(zone.state)})}}
