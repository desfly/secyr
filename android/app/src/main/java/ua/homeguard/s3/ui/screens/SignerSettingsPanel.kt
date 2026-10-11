package ua.homeguard.s3.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp

@Composable
fun SignerSettingsPanel(
    deviceId: String,
    savedUrl: String,
    configured: Boolean,
    onSave: (String, String) -> Boolean,
    onClear: () -> Unit,
) {
    var url by remember(deviceId, savedUrl) { mutableStateOf(savedUrl) }
    var token by remember(deviceId) { mutableStateOf("") }
    var error by remember(deviceId) { mutableStateOf(false) }
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Віддалене керування", style = MaterialTheme.typography.titleMedium)
            Text(if (configured) "Доступ налаштовано для цього контролера" else "Введіть адресу сервера та токен вашого облікового запису")
            OutlinedTextField(url, { url = it; error = false }, label = { Text("Адреса сервера HTTPS") },
                singleLine = true, modifier = Modifier.fillMaxWidth())
            OutlinedTextField(token, { token = it; error = false }, label = { Text("Токен доступу") },
                singleLine = true, visualTransformation = PasswordVisualTransformation(), modifier = Modifier.fillMaxWidth())
            if (error) Text("Перевірте HTTPS-адресу та токен доступу", color = MaterialTheme.colorScheme.error)
            Button(enabled = url.isNotBlank() && token.isNotBlank(), onClick = {
                if (onSave(url, token)) { token = ""; error = false } else error = true
            }) { Text("Зберегти доступ") }
            if (configured) OutlinedButton(onClick = { onClear(); token = ""; error = false }) { Text("Видалити доступ із телефона") }
        }
    }
}
