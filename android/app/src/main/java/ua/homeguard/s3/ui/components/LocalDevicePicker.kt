package ua.homeguard.s3.ui.components

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import ua.homeguard.s3.model.DiscoveredDevice

@Composable
fun LocalDevicePicker(
    devices: List<DiscoveredDevice>,
    busy: Boolean,
    onSearch: () -> Unit,
    onSelect: (DiscoveredDevice) -> Unit,
) {
    Column(
        modifier = Modifier.fillMaxWidth(),
        verticalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        Text(
            "HomeGuard у локальній мережі",
            style = MaterialTheme.typography.titleMedium,
            fontWeight = FontWeight.SemiBold,
        )
        Text(
            if (devices.isEmpty()) {
                "Натисніть пошук. Телефон знайде контролери HomeGuard-S3 через mDNS/UDP."
            } else {
                "Знайдено: ${devices.size}. Виберіть контролер для авторизації."
            },
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Button(
            onClick = onSearch,
            enabled = !busy,
            modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp),
        ) {
            Text(if (devices.isEmpty()) "Пошук HomeGuard" else "Шукати знову")
        }

        devices.forEach { device ->
            Button(
                onClick = { onSelect(device) },
                enabled = !busy,
                modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp),
            ) {
                Column(modifier = Modifier.fillMaxWidth()) {
                    Text(device.deviceId, fontWeight = FontWeight.Bold)
                    Text(
                        "${device.host}:${device.port} · ${device.source.name}",
                        style = MaterialTheme.typography.bodySmall,
                    )
                }
            }
        }
    }
}
