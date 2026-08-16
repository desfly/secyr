package ua.homeguard.s3

import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.dp
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.launch
import ua.homeguard.s3.diagnostics.DeviceConfigMaintenanceClient
import ua.homeguard.s3.model.AccessRole
import ua.homeguard.s3.model.AccessSession
import ua.homeguard.s3.model.ControlPath
import ua.homeguard.s3.model.DeviceEndpoint
import ua.homeguard.s3.network.HttpDeviceApi
import ua.homeguard.s3.storage.RegisteredDeviceStore
import ua.homeguard.s3.storage.SettingsStore

class ControllerMaintenanceActivity : ComponentActivity() {
    private lateinit var settings: SettingsStore
    private lateinit var registeredDevices: RegisteredDeviceStore
    private lateinit var endpoint: MutableStateFlow<DeviceEndpoint>
    private lateinit var maintenance: DeviceConfigMaintenanceClient
    private var pendingBackupText = ""
    private var pendingImportText = ""

    private val exportLauncher = registerForActivityResult(ActivityResultContracts.CreateDocument("application/json")) { uri ->
        val text = pendingBackupText
        pendingBackupText = ""
        if (uri == null || text.isEmpty()) return@registerForActivityResult
        runCatching {
            contentResolver.openOutputStream(uri)?.bufferedWriter(Charsets.UTF_8)?.use { it.write(text) }
                ?: error("cannot open destination")
        }
    }

    private val importLauncher = registerForActivityResult(ActivityResultContracts.GetContent()) { uri ->
        if (uri == null) return@registerForActivityResult
        runCatching {
            contentResolver.openInputStream(uri)?.bufferedReader(Charsets.UTF_8)?.use { it.readText() }
                ?: error("cannot read backup")
        }.onSuccess { pendingImportText = it }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        settings = SettingsStore(this)
        registeredDevices = RegisteredDeviceStore(this)
        val current = settings.settings.value
        endpoint = MutableStateFlow(
            DeviceEndpoint(
                deviceId = current.deviceId,
                apiBaseUrl = current.lastKnownLocalUrl,
                websocketUrl = "",
                path = if (current.lastKnownLocalUrl.isBlank()) ControlPath.OFFLINE else ControlPath.LAST_KNOWN_LOCAL,
                certificateSha256 = current.localCertificateSha256,
            ),
        )
        maintenance = DeviceConfigMaintenanceClient(endpoint)

        setContent {
            var actor by remember { mutableStateOf("") }
            var pin by remember { mutableStateOf("") }
            var pinVisible by remember { mutableStateOf(false) }
            var session by remember { mutableStateOf<AccessSession?>(null) }
            var status by remember { mutableStateOf("Увійдіть як Admin") }
            var confirmReset by remember { mutableStateOf(false) }
            var importReady by remember { mutableStateOf(false) }

            fun login() {
                lifecycleScope.launch {
                    status = "Перевірка доступу…"
                    runCatching {
                        require(endpoint.value.path != ControlPath.OFFLINE) { "Локальний контролер недоступний" }
                        val api = HttpDeviceApi(
                            baseUrl = endpoint.value.apiBaseUrl,
                            tokenProvider = { settings.settings.value.apiToken },
                            certificatePin = endpoint.value.certificateSha256,
                            runtimeV1 = true,
                        )
                        api.login(actor.trim(), pin)
                    }.onSuccess { authenticated ->
                        if (authenticated.role != AccessRole.ADMIN) {
                            session = null
                            status = "Потрібна роль Admin"
                        } else {
                            session = authenticated
                            status = "Admin: ${authenticated.name}"
                        }
                    }.onFailure { error ->
                        session = null
                        status = "Вхід відхилено: ${error.message ?: "network"}"
                    }
                }
            }

            if (confirmReset) {
                AlertDialog(
                    onDismissRequest = { confirmReset = false },
                    title = { Text("Повне заводське скидання?") },
                    text = {
                        Text(
                            "Будуть стерті користувачі, Wi-Fi, Cloud і всі користувацькі налаштування. " +
                                "Прошивка та hardware identity залишаться. Після команди контролер відключиться і перезавантажиться.",
                        )
                    },
                    confirmButton = {
                        TextButton(
                            onClick = {
                                confirmReset = false
                                val authenticated = session ?: return@TextButton
                                val resetDeviceId = endpoint.value.deviceId
                                lifecycleScope.launch {
                                    status = "Виконується Factory Reset…"
                                    runCatching { maintenance.factoryReset(authenticated, pin) }
                                        .onSuccess {
                                            if (resetDeviceId.isNotBlank()) {
                                                registeredDevices.markAuthorization(resetDeviceId, false)
                                            }
                                            settings.clearControllerSessionAfterFactoryReset()
                                            endpoint.value = DeviceEndpoint(
                                                deviceId = "",
                                                apiBaseUrl = "",
                                                websocketUrl = "",
                                                path = ControlPath.OFFLINE,
                                                certificateSha256 = "",
                                            )
                                            session = null
                                            pin = ""
                                            pinVisible = false
                                            status = "Factory Reset прийнято; контролер перезавантажується"
                                            startActivity(
                                                Intent(this@ControllerMaintenanceActivity, MainActivity::class.java).apply {
                                                    addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_NEW_TASK)
                                                },
                                            )
                                            finish()
                                        }
                                        .onFailure { error ->
                                            status = "Factory Reset: ${error.message ?: "network"}"
                                        }
                                }
                            },
                        ) { Text("СТЕРТИ ВСЕ") }
                    },
                    dismissButton = {
                        TextButton(onClick = { confirmReset = false }) { Text("Скасувати") }
                    },
                )
            }

            MaterialTheme {
                Column(
                    modifier = Modifier.padding(16.dp),
                    verticalArrangement = Arrangement.spacedBy(10.dp),
                ) {
                    Text("Конфігурація контролера", style = MaterialTheme.typography.headlineSmall)
                    Text(
                        endpoint.value.apiBaseUrl.ifBlank { "Локальний контролер не вибраний" },
                        style = MaterialTheme.typography.bodySmall,
                    )
                    OutlinedTextField(
                        value = actor,
                        onValueChange = {
                            actor = it.take(23)
                            session = null
                        },
                        modifier = Modifier.fillMaxWidth(),
                        label = { Text("Admin ID") },
                        singleLine = true,
                    )
                    OutlinedTextField(
                        value = pin,
                        onValueChange = { value ->
                            if (value.length <= 12 && value.all(Char::isDigit)) {
                                pin = value
                                session = null
                            }
                        },
                        modifier = Modifier.fillMaxWidth(),
                        label = { Text("PIN") },
                        singleLine = true,
                        visualTransformation = if (pinVisible) VisualTransformation.None else PasswordVisualTransformation(),
                        trailingIcon = {
                            TextButton(onClick = { pinVisible = !pinVisible }) {
                                Text(if (pinVisible) "Сховати" else "Показати")
                            }
                        },
                    )
                    Button(
                        enabled = actor.isNotBlank() && pin.length in 4..12,
                        onClick = { login() },
                        modifier = Modifier.fillMaxWidth(),
                    ) { Text("Увійти як Admin") }

                    OutlinedButton(
                        enabled = session?.role == AccessRole.ADMIN,
                        onClick = {
                            val authenticated = session ?: return@OutlinedButton
                            lifecycleScope.launch {
                                status = "Експорт конфігурації…"
                                runCatching { maintenance.exportConfig(authenticated, pin) }
                                    .onSuccess { text ->
                                        pendingBackupText = text
                                        exportLauncher.launch("homeguard-config-v1.json")
                                        status = "Backup готовий до збереження"
                                    }
                                    .onFailure { error ->
                                        status = "Export: ${error.message ?: "network"}"
                                    }
                            }
                        },
                        modifier = Modifier.fillMaxWidth(),
                    ) { Text("Експорт конфігурації") }

                    OutlinedButton(
                        enabled = session?.role == AccessRole.ADMIN,
                        onClick = {
                            pendingImportText = ""
                            importLauncher.launch("application/json")
                            importReady = true
                            status = "Оберіть homeguard-config-v1.json, потім натисніть «Застосувати імпорт»"
                        },
                        modifier = Modifier.fillMaxWidth(),
                    ) { Text("Обрати файл імпорту") }

                    OutlinedButton(
                        enabled = session?.role == AccessRole.ADMIN && importReady,
                        onClick = {
                            val authenticated = session ?: return@OutlinedButton
                            val backup = pendingImportText
                            if (backup.isBlank()) {
                                status = "Файл ще не вибраний або не прочитаний"
                                return@OutlinedButton
                            }
                            lifecycleScope.launch {
                                status = "Імпорт конфігурації…"
                                runCatching { maintenance.importConfig(authenticated, pin, backup) }
                                    .onSuccess {
                                        pendingImportText = ""
                                        importReady = false
                                        session = null
                                        pin = ""
                                        pinVisible = false
                                        status = "Імпорт прийнято; контролер перезавантажується"
                                    }
                                    .onFailure { error ->
                                        status = "Import: ${error.message ?: "network"}"
                                    }
                            }
                        },
                        modifier = Modifier.fillMaxWidth(),
                    ) { Text("Застосувати імпорт") }

                    OutlinedButton(
                        enabled = session?.role == AccessRole.ADMIN,
                        onClick = { confirmReset = true },
                        modifier = Modifier.fillMaxWidth(),
                    ) { Text("Повне заводське скидання") }

                    Text(status, style = MaterialTheme.typography.bodySmall)
                    OutlinedButton(
                        onClick = { finish() },
                        modifier = Modifier.fillMaxWidth(),
                    ) { Text("Назад") }
                }
            }
        }
    }
}
