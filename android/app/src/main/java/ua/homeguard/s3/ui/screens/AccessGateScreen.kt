package ua.homeguard.s3.ui.screens

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import ua.homeguard.s3.R
import ua.homeguard.s3.model.AccessLifecycleState

data class SetupWifiChoice(val ssid: String, val rssi: Int)

@Composable
fun AccessGateScreen(
    state: AccessLifecycleState,
    busy: Boolean,
    message: String,
    wifiNetworks: List<SetupWifiChoice>,
    onRetry: () -> Unit,
    onScanWifi: () -> Unit,
    onConnectWifi: (String, String) -> Unit,
    onBootstrapAdmin: (String, String, String) -> Unit,
    onLogin: (String, String) -> Unit,
    onBack: () -> Unit,
) {
    var actor by remember { mutableStateOf("") }
    var name by remember { mutableStateOf("") }
    var pin by remember { mutableStateOf("") }
    var wifiSsid by remember { mutableStateOf("") }
    var wifiPassword by remember { mutableStateOf("") }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color(0xFF07131F)),
    ) {
        Image(
            painter = painterResource(R.drawable.bruce_launcher),
            contentDescription = null,
            contentScale = ContentScale.Fit,
            modifier = Modifier.fillMaxSize(),
        )
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(Color(0x8807131F)),
        )
        Card(
            modifier = Modifier
                .align(Alignment.Center)
                .padding(20.dp)
                .fillMaxWidth()
                .verticalScroll(rememberScrollState()),
            colors = CardDefaults.cardColors(containerColor = Color(0xEE10243A)),
        ) {
            Column(
                modifier = Modifier.padding(20.dp),
                verticalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                Text("HomeGuard-S3", color = Color.White)
                when (state) {
                    AccessLifecycleState.UNAVAILABLE -> {
                        Text("Контролер недоступний або стан доступу ще не визначено.", color = Color.White)
                        if (busy) CircularProgressIndicator()
                        OutlinedButton(onClick = onRetry, enabled = !busy, modifier = Modifier.fillMaxWidth()) {
                            Text("Повторити")
                        }
                    }

                    AccessLifecycleState.SETUP_REQUIRED -> {
                        Text(
                            "Первинне налаштування можна відкривати необмежену кількість разів до успішного створення першого Admin.",
                            color = Color.White,
                        )
                        Text("1. Wi-Fi (необов'язково)", color = Color.White)
                        OutlinedButton(onClick = onScanWifi, enabled = !busy, modifier = Modifier.fillMaxWidth()) {
                            Text("Сканувати Wi-Fi")
                        }
                        wifiNetworks.forEach { item ->
                            OutlinedButton(
                                onClick = { wifiSsid = item.ssid },
                                enabled = !busy,
                                modifier = Modifier.fillMaxWidth(),
                            ) {
                                Text("${item.ssid.ifBlank { "(прихована мережа)" }} · ${item.rssi} dBm")
                            }
                        }
                        OutlinedTextField(
                            value = wifiSsid,
                            onValueChange = { wifiSsid = it.take(32) },
                            label = { Text("SSID") },
                            singleLine = true,
                            modifier = Modifier.fillMaxWidth(),
                        )
                        OutlinedTextField(
                            value = wifiPassword,
                            onValueChange = { wifiPassword = it.take(64) },
                            label = { Text("Пароль Wi-Fi") },
                            visualTransformation = PasswordVisualTransformation(),
                            singleLine = true,
                            modifier = Modifier.fillMaxWidth(),
                        )
                        OutlinedButton(
                            onClick = { onConnectWifi(wifiSsid.trim(), wifiPassword) },
                            enabled = !busy && wifiSsid.isNotBlank(),
                            modifier = Modifier.fillMaxWidth(),
                        ) { Text("Підключити Wi-Fi") }

                        Spacer(Modifier.height(8.dp))
                        Text("2. Перший адміністратор", color = Color.White)
                        OutlinedTextField(
                            value = actor,
                            onValueChange = { actor = it.take(23) },
                            label = { Text("ID адміністратора") },
                            singleLine = true,
                            modifier = Modifier.fillMaxWidth(),
                        )
                        OutlinedTextField(
                            value = name,
                            onValueChange = { name = it.take(31) },
                            label = { Text("Ім'я") },
                            singleLine = true,
                            modifier = Modifier.fillMaxWidth(),
                        )
                        OutlinedTextField(
                            value = pin,
                            onValueChange = { pin = it.filter(Char::isDigit).take(12) },
                            label = { Text("Пароль / PIN") },
                            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.NumberPassword),
                            visualTransformation = PasswordVisualTransformation(),
                            singleLine = true,
                            modifier = Modifier.fillMaxWidth(),
                        )
                        Button(
                            onClick = { onBootstrapAdmin(actor.trim(), name.trim(), pin) },
                            enabled = !busy && actor.isNotBlank() && name.isNotBlank() && pin.length in 4..12,
                            modifier = Modifier.fillMaxWidth(),
                        ) { Text("Створити Admin і закрити setup") }
                    }

                    AccessLifecycleState.LOGIN_REQUIRED -> {
                        Text("Введіть ім'я користувача / ID та пароль.", color = Color.White)
                        OutlinedTextField(
                            value = actor,
                            onValueChange = { actor = it.take(23) },
                            label = { Text("Користувач") },
                            singleLine = true,
                            modifier = Modifier.fillMaxWidth(),
                        )
                        OutlinedTextField(
                            value = pin,
                            onValueChange = { pin = it.filter(Char::isDigit).take(12) },
                            label = { Text("Пароль / PIN") },
                            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.NumberPassword),
                            visualTransformation = PasswordVisualTransformation(),
                            singleLine = true,
                            modifier = Modifier.fillMaxWidth(),
                        )
                        Button(
                            onClick = { onLogin(actor.trim(), pin) },
                            enabled = !busy && actor.isNotBlank() && pin.length in 4..12,
                            modifier = Modifier.fillMaxWidth(),
                        ) { Text("Увійти") }
                    }
                }

                if (busy && state != AccessLifecycleState.UNAVAILABLE) CircularProgressIndicator()
                if (message.isNotBlank()) Text(message, color = Color.White)
                OutlinedButton(onClick = onBack, enabled = !busy, modifier = Modifier.fillMaxWidth()) {
                    Text("До списку пристроїв")
                }
            }
        }
    }
}
