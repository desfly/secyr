package ua.homeguard.s3

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import com.journeyapps.barcodescanner.ScanContract
import com.journeyapps.barcodescanner.ScanOptions
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.launch
import ua.homeguard.s3.control.CommandController
import ua.homeguard.s3.diagnostics.SystemDiagnosticsEvaluator
import ua.homeguard.s3.events.EventLogExporter
import ua.homeguard.s3.model.AccessSession
import ua.homeguard.s3.model.CommandType
import ua.homeguard.s3.model.ProvisioningPhase
import ua.homeguard.s3.model.SystemSnapshot
import ua.homeguard.s3.network.DeviceEndpointResolver
import ua.homeguard.s3.network.DeviceSession
import ua.homeguard.s3.network.LocalDiscoveryCoordinator
import ua.homeguard.s3.network.TelemetrySocket
import ua.homeguard.s3.notifications.HomeGuardNotifications
import ua.homeguard.s3.repository.ProvisioningCoordinator
import ua.homeguard.s3.storage.EventHistoryStore
import ua.homeguard.s3.storage.SettingsBackupCodec
import ua.homeguard.s3.storage.SettingsStore
import ua.homeguard.s3.ui.screens.DashboardScreen
import ua.homeguard.s3.ui.screens.ProvisioningScreen

class MainActivity : ComponentActivity() {
    private lateinit var discovery: LocalDiscoveryCoordinator
    private lateinit var settings: SettingsStore
    private lateinit var eventHistory: EventHistoryStore
    private lateinit var resolver: DeviceEndpointResolver
    private lateinit var provisioning: ProvisioningCoordinator
    private lateinit var telemetry: TelemetrySocket
    private lateinit var session: DeviceSession
    private lateinit var commands: CommandController
    private lateinit var notifications: HomeGuardNotifications
    private val commandStatus = MutableStateFlow("Готово")
    private val backupStatus = MutableStateFlow("Backup/restore готовий")
    private val operatorId = MutableStateFlow("admin")
    private val operatorPin = MutableStateFlow("")
    private val accessSession = MutableStateFlow<AccessSession?>(null)
    private var pendingExportText: String = ""
    private var pendingSettingsBackupText: String = ""

    private val exportLauncher = registerForActivityResult(ActivityResultContracts.CreateDocument("text/csv")) { uri ->
        if (uri != null && pendingExportText.isNotEmpty()) {
            runCatching {
                contentResolver.openOutputStream(uri)?.bufferedWriter(Charsets.UTF_8)?.use { it.write(pendingExportText) }
            }
        }
        pendingExportText = ""
    }

    private val settingsBackupLauncher = registerForActivityResult(ActivityResultContracts.CreateDocument("application/json")) { uri ->
        if (uri == null || pendingSettingsBackupText.isEmpty()) {
            pendingSettingsBackupText = ""
            return@registerForActivityResult
        }
        val result = runCatching {
            contentResolver.openOutputStream(uri)?.bufferedWriter(Charsets.UTF_8)?.use { it.write(pendingSettingsBackupText) }
                ?: error("cannot open destination")
        }
        backupStatus.value = if (result.isSuccess) "Backup налаштувань збережено" else "Помилка backup: ${result.exceptionOrNull()?.message ?: "write"}"
        pendingSettingsBackupText = ""
    }

    private val settingsRestoreLauncher = registerForActivityResult(ActivityResultContracts.GetContent()) { uri ->
        if (uri == null) return@registerForActivityResult
        lifecycleScope.launch {
            val result = runCatching {
                val text = contentResolver.openInputStream(uri)?.bufferedReader(Charsets.UTF_8)?.use { it.readText() }
                    ?: error("cannot read backup")
                SettingsBackupCodec.decode(text, settings.settings.value.apiToken)
            }
            result.onSuccess { restored ->
                settings.update(restored)
                backupStatus.value = "Restore виконано; секретний API token збережено локально"
            }.onFailure { error ->
                backupStatus.value = "Помилка restore: ${error.message ?: "invalid backup"}"
            }
        }
    }

    private val qrScanner = registerForActivityResult(ScanContract()) { result ->
        result.contents?.let(provisioning::acceptQr)
    }

    private val permissionLauncher = registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {
        if (requiredProvisioningPermissions().all { permission ->
                ContextCompat.checkSelfPermission(this, permission) == PackageManager.PERMISSION_GRANTED
            }) launchQrScanner()
    }

    private val notificationPermissionLauncher = registerForActivityResult(ActivityResultContracts.RequestPermission()) { }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        settings = SettingsStore(this)
        eventHistory = EventHistoryStore(this)
        discovery = LocalDiscoveryCoordinator(this, lifecycleScope)
        resolver = DeviceEndpointResolver(settings, discovery, lifecycleScope)
        provisioning = ProvisioningCoordinator(this, settings, discovery, lifecycleScope)
        telemetry = TelemetrySocket().apply { seedEvents(eventHistory.load()) }
        session = DeviceSession(lifecycleScope, resolver.endpoint, settings, telemetry)
        commands = CommandController(resolver.endpoint, settings)
        notifications = HomeGuardNotifications(this)
        notifications.createChannels()
        requestNotificationPermission()
        lifecycleScope.launch {
            telemetry.liveEvents().collect { event ->
                eventHistory.append(event)
                notifications.notify(event, settings.settings.value)
            }
        }
        discovery.start()
        session.start()
        setContent {
            val appSettings by settings.settings.collectAsState()
            val devices by discovery.devices.collectAsState()
            val endpoint by resolver.endpoint.collectAsState()
            val provisioningState by provisioning.state.collectAsState()
            val snapshot by telemetry.snapshots().collectAsState(initial = SystemSnapshot())
            val events by telemetry.events().collectAsState(initial = emptyList())
            val commandMessage by commandStatus.collectAsState()
            val maintenanceMessage by backupStatus.collectAsState()
            val currentOperator by operatorId.collectAsState()
            val currentPin by operatorPin.collectAsState()
            val currentAccessSession by accessSession.collectAsState()
            val diagnostics = SystemDiagnosticsEvaluator.evaluate(
                deviceId = appSettings.deviceId,
                route = endpoint.path.name,
                localDevices = devices.size,
                certificateSha256 = appSettings.localCertificateSha256,
                snapshot = snapshot,
                eventCount = events.size,
            )
            MaterialTheme {
                val provisioningActive = provisioningState.phase in setOf(
                    ProvisioningPhase.CONNECTING_SETUP_AP,
                    ProvisioningPhase.AUTHORIZING,
                    ProvisioningPhase.APPLYING,
                    ProvisioningPhase.WAITING_FOR_RESTART,
                    ProvisioningPhase.DISCOVERING_LOCAL
                )
                if (appSettings.deviceId.isBlank() || provisioningActive) {
                    ProvisioningScreen(
                        state = provisioningState,
                        onScan = ::requestQrScan,
                        onProvision = provisioning::provision
                    )
                } else {
                    DashboardScreen(
                        versionName = BuildConfig.VERSION_NAME,
                        localDevices = devices.size,
                        route = endpoint.path.name,
                        deviceId = appSettings.deviceId,
                        snapshot = snapshot,
                        events = events,
                        diagnostics = diagnostics,
                        backupStatus = maintenanceMessage,
                        commandStatus = commandMessage,
                        operatorId = currentOperator,
                        operatorPin = currentPin,
                        accessSession = currentAccessSession,
                        criticalNotificationsEnabled = appSettings.criticalNotificationsEnabled,
                        statusNotificationsEnabled = appSettings.statusNotificationsEnabled,
                        zoneNotificationsEnabled = appSettings.zoneNotificationsEnabled,
                        onOperatorIdChange = { value ->
                            operatorId.value = value.take(23)
                            accessSession.value = null
                        },
                        onOperatorPinChange = { value ->
                            operatorPin.value = value.filter(Char::isDigit).take(12)
                            accessSession.value = null
                        },
                        onLogin = ::loginOperator,
                        onLogout = ::logoutOperator,
                        onCriticalNotificationsChange = { enabled -> lifecycleScope.launch { settings.update(settings.settings.value.copy(criticalNotificationsEnabled = enabled)) } },
                        onStatusNotificationsChange = { enabled -> lifecycleScope.launch { settings.update(settings.settings.value.copy(statusNotificationsEnabled = enabled)) } },
                        onZoneNotificationsChange = { enabled -> lifecycleScope.launch { settings.update(settings.settings.value.copy(zoneNotificationsEnabled = enabled)) } },
                        onClearEventHistory = {
                            eventHistory.clear()
                            telemetry.clearEvents()
                        },
                        onExportEvents = {
                            pendingExportText = EventLogExporter.toCsv(events)
                            exportLauncher.launch(EventLogExporter.suggestedFileName())
                        },
                        onShareEvents = {
                            val payload = EventLogExporter.toCsv(events)
                            val intent = Intent(Intent.ACTION_SEND).apply {
                                type = "text/csv"
                                putExtra(Intent.EXTRA_SUBJECT, "HomeGuard-S3 event log")
                                putExtra(Intent.EXTRA_TEXT, payload)
                            }
                            startActivity(Intent.createChooser(intent, "Поділитися журналом"))
                        },
                        onExportSettings = {
                            pendingSettingsBackupText = SettingsBackupCodec.encode(appSettings)
                            settingsBackupLauncher.launch(SettingsBackupCodec.suggestedFileName())
                        },
                        onImportSettings = { settingsRestoreLauncher.launch("application/json") },
                        onCommand = ::executeCommand,
                    )
                }
            }
        }
    }

    private fun loginOperator() {
        val actor = operatorId.value.trim()
        val credential = operatorPin.value
        if (actor.isBlank() || credential.length !in 4..12 || !credential.all(Char::isDigit)) {
            commandStatus.value = "Введіть ID користувача та PIN 4–12 цифр"
            accessSession.value = null
            return
        }

        lifecycleScope.launch {
            commandStatus.value = "Перевірка доступу…"
            val result = runCatching { commands.login(actor, credential) }
            result.onSuccess { authenticated ->
                accessSession.value = authenticated
                commandStatus.value = "Вхід: ${authenticated.name} · ${authenticated.role.name.lowercase()}"
            }.onFailure { error ->
                accessSession.value = null
                commandStatus.value = "Вхід відхилено: ${error.message ?: "network"}"
            }
        }
    }

    private fun logoutOperator() {
        accessSession.value = null
        operatorPin.value = ""
        commandStatus.value = "Сеанс завершено"
    }

    private fun executeCommand(type: CommandType) {
        val actor = operatorId.value.trim()
        val credential = operatorPin.value
        val authenticated = accessSession.value
        if (authenticated == null || authenticated.actor != actor) {
            commandStatus.value = "Спочатку увійдіть"
            return
        }
        if (!authenticated.allows(type)) {
            commandStatus.value = "Недоступно для ролі ${authenticated.role.name.lowercase()}"
            return
        }
        if (credential.length !in 4..12 || !credential.all(Char::isDigit)) {
            accessSession.value = null
            commandStatus.value = "PIN сеансу відсутній — увійдіть знову"
            return
        }

        lifecycleScope.launch {
            commandStatus.value = "Виконується: ${type.name}…"
            val result = runCatching { commands.execute(type, actor, credential) }
            commandStatus.value = result.fold(
                onSuccess = { reply ->
                    if (reply.accepted || reply.duplicate) "OK: ${reply.code}"
                    else {
                        if (reply.code.contains("unauthorized", ignoreCase = true) ||
                            reply.code.contains("credential", ignoreCase = true) ||
                            reply.code.contains("rate", ignoreCase = true)
                        ) accessSession.value = null
                        "Відхилено: ${reply.code}"
                    }
                },
                onFailure = { error -> "Помилка: ${error.message ?: "network"}" },
            )
        }
    }

    private fun requestNotificationPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
            ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED
        ) notificationPermissionLauncher.launch(Manifest.permission.POST_NOTIFICATIONS)
    }

    private fun requestQrScan() {
        val missing = requiredProvisioningPermissions().filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }
        if (missing.isEmpty()) launchQrScanner() else permissionLauncher.launch(missing.toTypedArray())
    }

    private fun launchQrScanner() {
        qrScanner.launch(ScanOptions().setPrompt("Скануйте QR HomeGuard-S3").setBeepEnabled(false).setOrientationLocked(false))
    }

    private fun requiredProvisioningPermissions(): List<String> = buildList {
        add(Manifest.permission.CAMERA)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) add(Manifest.permission.NEARBY_WIFI_DEVICES)
        else add(Manifest.permission.ACCESS_FINE_LOCATION)
    }

    override fun onDestroy() {
        accessSession.value = null
        operatorPin.value = ""
        session.stop()
        discovery.stop()
        super.onDestroy()
    }
}
