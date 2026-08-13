package ua.homeguard.s3.ui.screens

import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.unit.dp
import ua.homeguard.s3.model.RegisteredDevice
import ua.homeguard.s3.model.RegisteredDeviceAccess

@Composable
fun RegisteredDevicesScreen(
    devices: List<RegisteredDevice>,
    onAddDevice: () -> Unit,
    onOpenDevice: (RegisteredDevice) -> Unit,
) {
    var expandedId by remember { mutableStateOf<String?>(null) }

    Column(Modifier.fillMaxSize().padding(16.dp)) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text("Мої пристрої · ${devices.size}", style = MaterialTheme.typography.headlineSmall)
            Button(onClick = onAddDevice) { Text("Додати") }
        }
        Spacer(Modifier.height(12.dp))
        if (devices.isEmpty()) {
            Text("У вас ще немає пристроїв")
            Spacer(Modifier.height(12.dp))
            Button(onClick = onAddDevice, modifier = Modifier.fillMaxWidth()) { Text("Додати пристрій") }
            return@Column
        }

        LazyColumn(verticalArrangement = Arrangement.spacedBy(10.dp)) {
            items(devices, key = { it.deviceId }) { device ->
                val expanded = expandedId == device.deviceId
                DeviceCard(
                    device = device,
                    expanded = expanded,
                    onTap = { expandedId = if (expanded) null else device.deviceId },
                    onDoubleTap = { onOpenDevice(device) },
                )
            }
        }
    }
}

@Composable
private fun DeviceCard(
    device: RegisteredDevice,
    expanded: Boolean,
    onTap: () -> Unit,
    onDoubleTap: () -> Unit,
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .pointerInput(device.deviceId) {
                detectTapGestures(onTap = { onTap() }, onDoubleTap = { onDoubleTap() })
            }
    ) {
        Column(Modifier.padding(16.dp)) {
            Text(
                text = device.displayName,
                style = MaterialTheme.typography.titleLarge,
                color = if (device.accessRevoked) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.onSurface,
            )
            Text(connectionLabel(device.access), style = MaterialTheme.typography.bodyMedium)
            if (expanded) {
                Spacer(Modifier.height(10.dp))
                if (device.accessRevoked) {
                    Text("Доступ відкликано адміністратором", color = MaterialTheme.colorScheme.error)
                } else {
                    Text("Охорона: —")
                    Text("Аварії: —")
                }
            }
        }
    }
}

private fun connectionLabel(access: RegisteredDeviceAccess): String = when (access) {
    RegisteredDeviceAccess.ONLINE -> "Онлайн"
    RegisteredDeviceAccess.OFFLINE -> "Офлайн"
    RegisteredDeviceAccess.CREDENTIALS_REJECTED -> "Потрібна повторна авторизація"
    RegisteredDeviceAccess.ACCESS_REVOKED -> "Доступ відкликано"
    RegisteredDeviceAccess.UNKNOWN -> "Перевірка зв’язку…"
}
