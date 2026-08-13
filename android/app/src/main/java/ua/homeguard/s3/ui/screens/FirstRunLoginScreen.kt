package ua.homeguard.s3.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Button
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.dp
import ua.homeguard.s3.BuildConfig
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

@Composable
fun FirstRunLoginScreen(onContinue: (String, String) -> Unit) {
    var username by remember { mutableStateOf("") }
    var password by remember { mutableStateOf("") }
    var passwordVisible by remember { mutableStateOf(false) }
    val buildDate = remember {
        SimpleDateFormat("dd.MM.yyyy HH:mm", Locale.getDefault()).format(Date(BuildConfig.BUILD_TIME))
    }

    Column(
        modifier = Modifier.fillMaxSize().padding(horizontal = 24.dp),
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Text("MyFist", style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(4.dp))
        Text("Версія: ${BuildConfig.VERSION_NAME}", style = MaterialTheme.typography.bodyMedium)
        Text("Збірка: $buildDate", style = MaterialTheme.typography.bodyMedium)
        Spacer(Modifier.height(8.dp))
        Text("Перший вхід", style = MaterialTheme.typography.titleMedium)
        Spacer(Modifier.height(20.dp))
        OutlinedTextField(
            modifier = Modifier.fillMaxWidth(),
            value = username,
            onValueChange = { username = it.take(32) },
            label = { Text("Користувач") },
            singleLine = true,
        )
        Spacer(Modifier.height(12.dp))
        OutlinedTextField(
            modifier = Modifier.fillMaxWidth(),
            value = password,
            onValueChange = { password = it.take(64) },
            label = { Text("Пароль") },
            singleLine = true,
            visualTransformation = if (passwordVisible) VisualTransformation.None else PasswordVisualTransformation(),
            keyboardOptions = KeyboardOptions(keyboardType = if (passwordVisible) KeyboardType.Text else KeyboardType.Password),
            trailingIcon = {
                IconButton(onClick = { passwordVisible = !passwordVisible }) {
                    Text(if (passwordVisible) "Сховати" else "Показати")
                }
            },
        )
        Spacer(Modifier.height(20.dp))
        Button(
            modifier = Modifier.fillMaxWidth(),
            enabled = username.isNotBlank() && password.isNotBlank(),
            onClick = { onContinue(username.trim(), password) },
        ) { Text("Продовжити") }
    }
}
