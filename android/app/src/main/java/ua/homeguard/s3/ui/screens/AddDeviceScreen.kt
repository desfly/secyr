package ua.homeguard.s3.ui.screens

import android.content.Context
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import android.net.NetworkInfo
import android.os.Build
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
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.network.LocalDiscoveryProgress
import java.net.Inet4Address
import java.net.NetworkInterface

enum class AddDeviceMethod { ID, IP, NETWORK }

private enum class LocalNetworkState { WIFI, OTHER, OFFLINE }

@Suppress("DEPRECATION")
private fun localNetworkState(context: Context): LocalNetworkState {
    val manager = context.applicationContext
        .getSystemService(Context.CONNECTIVITY_SERVICE) as? ConnectivityManager
        ?: return LocalNetworkState.OFFLINE

    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
        val network = manager.activeNetwork ?: return LocalNetworkState.OFFLINE
        val capabilities = manager.getNetworkCapabilities(network) ?: return LocalNetworkState.OFFLINE
        return when {
            capabilities.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) -> LocalNetworkState.WIFI
            capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET) -> LocalNetworkState.OTHER
            else -> LocalNetworkState.OFFLINE
        }
    }

    val info: NetworkInfo = manager.activeNetworkInfo ?: return LocalNetworkState.OFFLINE
    if (!info.isConnected) return LocalNetworkState.OFFLINE
    return if (info.type == ConnectivityManager.TYPE_WIFI) LocalNetworkState.WIFI else LocalNetworkState.OTHER
}

private fun localIpv4Address(): String? = runCatching {
    val interfaces = NetworkInterface.getNetworkInterfaces() ?: return@runCatching null
    interfaces.toList()
        .asSequence()
        .filter { it.isUp && !it.isLoopback }
        .flatMap { it.inetAddresses.toList().asSequence() }
        .filterIsInstance<Inet4Address>()
        .map { it.hostAddress.orEmpty() }
        .firstOrNull { address ->
            val parts = address.split('.')
            parts.size == 4 && parts[0] != "127" && parts[0] != "169"
        }
}.getOrNull()

private fun setupGatewayFor(address: String?): String? {
    val parts = address?.split('.') ?: return null
    if (parts.size != 4 || parts[2] != "4") return null
    return "${parts[0]}.${parts[1]}.4.1"
}

@Composable
fun AddDeviceScreen(
    discoveredDevices: List<DiscoveredDevice>,
    onAddById: (name: String, deviceId: String) -> Unit,
    onAddByIp: (name: String, ip: String) -> Unit,
    onAddDiscovered: (name: String, device: DiscoveredDevice) -> Unit,
    onRescan: suspend () -> Unit,
    onBack: () -> Unit,
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val discoveryProgress by LocalDiscoveryProgress.state.collectAsState()
    var method by remember { mutableStateOf(AddDeviceMethod.NETWORK) }
    var name by remember { mutableStateOf("") }
    var value by remember { mutableStateOf("") }
    val networkState = localNetworkState(context)

    fun effectiveName(): String = name.trim().ifBlank { "HomeGuard-S3" }

    fun startSearch() {
        if (networkState != LocalNetworkState.WIFI || discoveryProgress.running) return
        scope.launch { runCatching { onRescan() } }
    }

    LaunchedEffect(networkState) {
        if (networkState == LocalNetworkState.WIFI) {
            val gateway = setupGatewayFor(localIpv4Address())
            if (gateway != null) {
                value = gateway
                method = AddDeviceMethod.IP
                runCatching { onRescan() }
            }
        }
    }

    LaunchedEffect(method, networkState) {
        if (method == AddDeviceMethod.NETWORK && networkState == LocalNetworkState.WIFI && discoveredDevices.isEmpty()) {
            startSearch()
        }
    }

    Column(
        modifier = Modifier.fillMaxSize().safeDrawingPadding().padding(horizontal = 16.dp, vertical = 10.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Text("Додати пристрій", style = MaterialTheme.typography.titleLarge)

        OutlinedTextField(
            value = name,
            onValueChange = { name = it.take(48) },
            label = { Text("Назва пристрою") },
            placeholder = { Text("HomeGuard-S3") },
            singleLine = true,
            modifier = Modifier.fillMaxWidth(),
        )

        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            FilterChip(selected = method == AddDeviceMethod.NETWORK, onClick = { method = AddDeviceMethod.NETWORK }, label = { Text("Мережа") })
            FilterChip(selected = method == AddDeviceMethod.IP, onClick = { method = AddDeviceMethod.IP }, label = { Text("IP") })
            FilterChip(selected = method == AddDeviceMethod.ID, onClick = { method = AddDeviceMethod.ID }, label = { Text("ID") })
        }

        when (method) {
            AddDeviceMethod.ID -> {
                OutlinedTextField(value = value, onValueChange = { value = it.trim().take(96) }, label = { Text("ID пристрою") }, singleLine = true, modifier = Modifier.fillMaxWidth())
                Button(enabled = value.isNotBlank(), onClick = { onAddById(effectiveName(), value.trim()) }) { Text("Додати") }
            }

            AddDeviceMethod.IP -> {
                OutlinedTextField(value = value, onValueChange = { value = it.trim().take(128) }, label = { Text("IP або адреса") }, placeholder = { Text("192.168.4.1") }, singleLine = true, modifier = Modifier.fillMaxWidth())
                Button(onClick = {
                    val address = value.trim()
                    if (address.isNotEmpty()) onAddByIp(effectiveName(), address)
                }) { Text("Підключити") }
                if (value.trim().endsWith(".4.1")) {
                    Text("Режим налаштування HomeGuard-S3 визначено автоматично. Виконується пошук контролера в локальній мережі.", style = MaterialTheme.typography.bodySmall)
                }
            }

            AddDeviceMethod.NETWORK -> {
                Text("Знайдено: ${discoveredDevices.size}", style = MaterialTheme.typography.titleMedium)
                if (networkState != LocalNetworkState.WIFI) {
                    Card(modifier = Modifier.fillMaxWidth()) {
                        Text("Для автоматичного пошуку підключіться до Wi-Fi", modifier = Modifier.padding(12.dp), style = MaterialTheme.typography.bodyMedium)
                    }
                } else {
                    if (discoveryProgress.running) {
                        Text(
                            "${discoveryProgress.phase} · ${(discoveryProgress.fraction * 100).toInt()}%",
                            style = MaterialTheme.typography.bodyMedium,
                        )
                        LinearProgressIndicator(
                            progress = { discoveryProgress.fraction },
                            modifier = Modifier.fillMaxWidth(),
                        )
                    } else {
                        Button(onClick = { startSearch() }) { Text(if (discoveredDevices.isEmpty()) "Шукати" else "Шукати знову") }
                        if (discoveredDevices.isEmpty() && discoveryProgress.fraction >= 1f) {
                            Text("Пристроїв не знайдено", style = MaterialTheme.typography.bodyMedium)
                        }
                    }

                    if (discoveredDevices.isNotEmpty()) {
                        LazyColumn(modifier = Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                            items(discoveredDevices, key = { it.deviceId }) { device ->
                                Card(modifier = Modifier.fillMaxWidth()) {
                                    Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                                        Text(device.serviceName.ifBlank { "HomeGuard-S3" }, style = MaterialTheme.typography.titleMedium)
                                        Text("${device.host}:${device.port}", style = MaterialTheme.typography.bodySmall)
                                        Spacer(Modifier.height(2.dp))
                                        Button(onClick = { onAddDiscovered(effectiveName(), device) }) { Text("Додати") }
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
