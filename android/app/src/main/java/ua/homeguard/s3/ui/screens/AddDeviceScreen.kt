package ua.homeguard.s3.ui.screens

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.net.ConnectivityManager
import android.net.NetworkInfo
import android.os.Build
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.FilterChip
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat
import kotlinx.coroutines.delay
import ua.homeguard.s3.model.DiscoveredDevice

enum class AddDeviceMethod { ID, IP, NETWORK }

private enum class LocalNetworkState { WIFI, OTHER, OFFLINE }

@Suppress("DEPRECATION")
private fun localNetworkState(context: Context): LocalNetworkState {
    val manager = context.applicationContext
        .getSystemService(Context.CONNECTIVITY_SERVICE) as? ConnectivityManager
        ?: return LocalNetworkState.OFFLINE
    val info: NetworkInfo = manager.activeNetworkInfo ?: return LocalNetworkState.OFFLINE
    if (!info.isConnected) return LocalNetworkState.OFFLINE
    return if (info.type == ConnectivityManager.TYPE_WIFI) LocalNetworkState.WIFI else LocalNetworkState.OTHER
}

@Composable
fun AddDeviceScreen(
    discoveredDevices: List<DiscoveredDevice>,
    onAddById: (name: String, deviceId: String) -> Unit,
    onAddByIp: (name: String, ip: String) -> Unit,
    onAddDiscovered: (name: String, device: DiscoveredDevice) -> Unit,
    onBack: () -> Unit,
) {
    val context = LocalContext.current
    val discoveryPermission = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        Manifest.permission.NEARBY_WIFI_DEVICES
    } else {
        Manifest.permission.ACCESS_FINE_LOCATION
    }
    var permissionGranted by remember {
        mutableStateOf(
            Build.VERSION.SDK_INT < Build.VERSION_CODES.M ||
                ContextCompat.checkSelfPermission(context, discoveryPermission) == PackageManager.PERMISSION_GRANTED,
        )
    }
    val permissionLauncher = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
        permissionGranted = granted
    }

    LaunchedEffect(discoveryPermission) {
        if (!permissionGranted) permissionLauncher.launch(discoveryPermission)
    }

    var method by remember { mutableStateOf(AddDeviceMethod.NETWORK) }
    var name by remember { mutableStateOf("") }
    var value by remember { mutableStateOf("") }
    var searchRunning by remember { mutableStateOf(false) }
    var searchProgress by remember { mutableFloatStateOf(0f) }
    val networkState = localNetworkState(context)

    fun startSearch() {
        if (networkState == LocalNetworkState.WIFI && permissionGranted) {
            searchProgress = 0f
            searchRunning = true
        }
    }

    LaunchedEffect(searchRunning) {
        if (!searchRunning) return@LaunchedEffect
        repeat(20) { step ->
            searchProgress = (step + 1) / 20f
            delay(400L)
        }
        searchRunning = false
    }

    LaunchedEffect(method, permissionGranted, networkState) {
        if (method == AddDeviceMethod.NETWORK && permissionGranted && networkState == LocalNetworkState.WIFI && discoveredDevices.isEmpty()) {
            startSearch()
        }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .safeDrawingPadding()
            .padding(horizontal = 16.dp, vertical = 10.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Text("Додати пристрій", style = MaterialTheme.typography.titleLarge)
        Text(
            "Вкажіть свою назву. Технічний ID у списку не показується.",
            style = MaterialTheme.typography.bodySmall,
        )

        OutlinedTextField(
            value = name,
            onValueChange = { name = it.take(48) },
            label = { Text("Назва пристрою") },
            singleLine = true,
            modifier = Modifier.fillMaxWidth(),
        )

        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            FilterChip(
                selected = method == AddDeviceMethod.NETWORK,
                onClick = { method = AddDeviceMethod.NETWORK },
                label = { Text("Мережа") },
            )
            FilterChip(
                selected = method == AddDeviceMethod.IP,
                onClick = { method = AddDeviceMethod.IP },
                label = { Text("IP") },
            )
            FilterChip(
                selected = method == AddDeviceMethod.ID,
                onClick = { method = AddDeviceMethod.ID },
                label = { Text("ID") },
            )
        }

        when (method) {
            AddDeviceMethod.ID -> {
                OutlinedTextField(
                    value = value,
                    onValueChange = { value = it.trim().take(96) },
                    label = { Text("ID пристрою") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
                Button(
                    enabled = name.isNotBlank() && value.isNotBlank(),
                    onClick = { onAddById(name.trim(), value.trim()) },
                ) { Text("Додати") }
            }

            AddDeviceMethod.IP -> {
                OutlinedTextField(
                    value = value,
                    onValueChange = { value = it.trim().take(128) },
                    label = { Text("IP або адреса") },
                    placeholder = { Text("192.168.1.50") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth(),
                )
                Button(
                    enabled = name.isNotBlank() && value.isNotBlank(),
                    onClick = { onAddByIp(name.trim(), value.trim()) },
                ) { Text("Перевірити й додати") }
            }

            AddDeviceMethod.NETWORK -> {
                Text("Знайдено: ${discoveredDevices.size}", style = MaterialTheme.typography.titleMedium)

                if (!permissionGranted) {
                    Card(modifier = Modifier.fillMaxWidth()) {
                        Column(
                            modifier = Modifier.padding(12.dp),
                            verticalArrangement = Arrangement.spacedBy(6.dp),
                        ) {
                            Text("Потрібен доступ до пристроїв поруч", style = MaterialTheme.typography.bodyMedium)
                            OutlinedButton(onClick = { permissionLauncher.launch(discoveryPermission) }) {
                                Text("Дозволити")
                            }
                        }
                    }
                } else if (networkState != LocalNetworkState.WIFI) {
                    Card(modifier = Modifier.fillMaxWidth()) {
                        Text(
                            "Для автоматичного пошуку підключіться до Wi-Fi",
                            modifier = Modifier.padding(12.dp),
                            style = MaterialTheme.typography.bodyMedium,
                        )
                    }
                } else {
                    if (searchRunning) {
                        Text("Пошук пристроїв… ${(searchProgress * 100).toInt()}%", style = MaterialTheme.typography.bodyMedium)
                        LinearProgressIndicator(
                            progress = searchProgress,
                            modifier = Modifier.fillMaxWidth(),
                        )
                    } else {
                        Button(onClick = { startSearch() }) {
                            Text(if (discoveredDevices.isEmpty()) "Шукати" else "Шукати знову")
                        }
                        if (discoveredDevices.isEmpty() && searchProgress >= 1f) {
                            Text("Пристроїв не знайдено", style = MaterialTheme.typography.bodyMedium)
                        }
                    }

                    if (discoveredDevices.isNotEmpty()) {
                        LazyColumn(
                            modifier = Modifier.weight(1f),
                            verticalArrangement = Arrangement.spacedBy(8.dp),
                        ) {
                            items(discoveredDevices, key = { it.deviceId }) { device ->
                                Card(modifier = Modifier.fillMaxWidth()) {
                                    Column(
                                        Modifier.padding(12.dp),
                                        verticalArrangement = Arrangement.spacedBy(4.dp),
                                    ) {
                                        Text(
                                            device.serviceName.ifBlank { "HomeGuard-S3" },
                                            style = MaterialTheme.typography.titleMedium,
                                        )
                                        Text("${device.host}:${device.port}", style = MaterialTheme.typography.bodySmall)
                                        Spacer(Modifier.height(2.dp))
                                        Button(
                                            enabled = name.isNotBlank(),
                                            onClick = { onAddDiscovered(name.trim(), device) },
                                        ) { Text(if (name.isBlank()) "Введіть назву" else "Додати") }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Spacer(Modifier.height(2.dp))
        OutlinedButton(onClick = onBack) { Text("Назад") }
    }
}
