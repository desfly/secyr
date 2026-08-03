package ua.homeguard.s3.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import ua.homeguard.s3.model.ProvisioningForm
import ua.homeguard.s3.model.ProvisioningPhase
import ua.homeguard.s3.model.ProvisioningUiState

@Composable
fun ProvisioningScreen(state: ProvisioningUiState, onScan: () -> Unit, onProvision: (ProvisioningForm) -> Unit) {
    var form by remember { mutableStateOf(ProvisioningForm()) }
    val busy = state.phase in setOf(ProvisioningPhase.CONNECTING_SETUP_AP, ProvisioningPhase.AUTHORIZING, ProvisioningPhase.APPLYING, ProvisioningPhase.WAITING_FOR_RESTART, ProvisioningPhase.DISCOVERING_LOCAL)
    Column(
        modifier = Modifier.fillMaxSize().padding(20.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        Text("HomeGuard-S3 — перше налаштування")
        Text(state.message)
        if (state.error.isNotBlank()) Text("Помилка: ${state.error}")
        if (state.localUrl.isNotBlank()) Text("Локальна адреса: ${state.localUrl}")
        Button(onClick = onScan, enabled = !busy) { Text("Сканувати QR на пристрої") }
        state.qr?.let {
            Text("Пристрій: ${it.deviceId}")
            Text("Setup AP: ${it.setupSsid}")
        }
        OutlinedTextField(form.wifiSsid, { form = form.copy(wifiSsid = it) }, label = { Text("Домашня Wi-Fi мережа") }, modifier = Modifier.fillMaxWidth())
        OutlinedTextField(
            form.wifiPassword,
            { form = form.copy(wifiPassword = it) },
            label = { Text("Пароль домашнього Wi-Fi") },
            modifier = Modifier.fillMaxWidth(),
            visualTransformation = PasswordVisualTransformation(),
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password)
        )
        OutlinedTextField(form.ownerLabel, { form = form.copy(ownerLabel = it) }, label = { Text("Назва об’єкта") }, modifier = Modifier.fillMaxWidth())
        OutlinedTextField(form.cloudEndpoint, { form = form.copy(cloudEndpoint = it) }, label = { Text("MQTTS адреса хмари — необов’язково") }, modifier = Modifier.fillMaxWidth())
        OutlinedTextField(
            form.cloudClaimToken,
            { form = form.copy(cloudClaimToken = it) },
            label = { Text("Одноразовий cloud claim token") },
            modifier = Modifier.fillMaxWidth(),
            visualTransformation = PasswordVisualTransformation()
        )
        Button(onClick = { onProvision(form) }, enabled = state.qr != null && !busy) { Text("Прив’язати HomeGuard-S3") }
        if (busy) CircularProgressIndicator()
    }
}
