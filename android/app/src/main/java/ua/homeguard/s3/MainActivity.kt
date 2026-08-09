package ua.homeguard.s3

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
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
import ua.homeguard.s3.model.CommandType
import ua.homeguard.s3.model.ProvisioningPhase
import ua.homeguard.s3.model.SystemSnapshot
import ua.homeguard.s3.network.CloudStateMqttClient
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
import ua.homeguard.s3.ui.theme.MyfistBackground
import ua.homeguard.s3.ui.theme.MyfistTheme

class MainActivity : ComponentActivity() {
    private lateinit var discovery: LocalDiscoveryCoordinator
    private lateinit var settings: SettingsStore
    private lateinit var eventHistory: EventHistoryStore
    private lateinit var resolver: DeviceEndpointResolver
    private lateinit var provisioning: ProvisioningCoordinator
    private lateinit var telemetry: TelemetrySocket
    private lateinit var cloudState: CloudStateMqttClient
    private lateinit var session: DeviceSession
    private lateinit var commands: CommandController
    private lateinit var notifications: HomeGuardNotifications
    private val commandStatus = MutableStateFlow("Готово")
    private val backupStatus = MutableStateFlow("Backup/restore готовий")
    private val operatorId = MutableStateFlow("admin")
    private val operatorPin = MutableStateFlow("")
    private var pendingExportText: String = ""
    private var pendingSettingsBackupText: String = ""

    private val exportLauncher = registerForActivityResult(ActivityResultContracts.CreateDocument("text/csv")) { uri ->
        if (uri != null && pendingExportText.isNotEmpty()) runCatching { contentResolver.openOutputStream(uri)?.bufferedWriter(Charsets.UTF_8)?.use { it.write(pendingExportText) } }
        pendingExportText = ""
    }
    private val settingsBackupLauncher = registerForActivityResult(ActivityResultContracts.CreateDocument("application/json")) { uri ->
        if (uri == null || pendingSettingsBackupText.isEmpty()) { pendingSettingsBackupText = ""; return@registerForActivityResult }
        val result = runCatching { contentResolver.openOutputStream(uri)?.bufferedWriter(Charsets.UTF_8)?.use { it.write(pendingSettingsBackupText) } ?: error("cannot open destination") }
        backupStatus.value = if (result.isSuccess) "Backup налаштувань збережено" else "Помилка backup: ${result.exceptionOrNull()?.message ?: "write"}"
        pendingSettingsBackupText = ""
    }
    private val settingsRestoreLauncher = registerForActivityResult(ActivityResultContracts.GetContent()) { uri ->
        if (uri == null) return@registerForActivityResult
        lifecycleScope.launch {
            val result = runCatching { val text = contentResolver.openInputStream(uri)?.bufferedReader(Charsets.UTF_8)?.use { it.readText() } ?: error("cannot read backup"); SettingsBackupCodec.decode(text, settings.settings.value.apiToken) }
            result.onSuccess { restored -> settings.update(restored); backupStatus.value = "Restore виконано; секретний API token збережено локально" }
                .onFailure { error -> backupStatus.value = "Помилка restore: ${error.message ?: "invalid backup"}" }
        }
    }
    private val qrScanner = registerForActivityResult(ScanContract()) { result -> result.contents?.let(provisioning::acceptQr) }
    private val permissionLauncher = registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {
        if (requiredProvisioningPermissions().all { permission -> ContextCompat.checkSelfPermission(this, permission) == PackageManager.PERMISSION_GRANTED }) launchQrScanner()
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
        cloudState = CloudStateMqttClient(telemetry)
        session = DeviceSession(lifecycleScope, resolver.endpoint, settings, telemetry, cloudState)
        commands = CommandController(resolver.endpoint, settings, cloudState)
        notifications = HomeGuardNotifications(this)
        notifications.createChannels()
        requestNotificationPermission()
        lifecycleScope.launch { telemetry.liveEvents().collect { event -> eventHistory.append(event); notifications.notify(event, settings.settings.value) } }
        discovery.start()
        session.start()
        setContent {
            val appSettings by settings.settings.collectAsState()
            val devices by discovery.devices.collectAsState()
            val endpoint by resolver.endpoint.collectAsState()
            val provisioningState by provisioning.state.collectAsState()
            val wifiNetworks by provisioning.wifiNetworks.collectAsState()
            val snapshot by telemetry.snapshots().collectAsState(initial = SystemSnapshot())
            val events by telemetry.events().collectAsState(initial = emptyList())
            val cloudStatus by cloudState.status.collectAsState()
            val cloudProfile by cloudState.profile.collectAsState()
            val commandMessage by commandStatus.collectAsState()
            val maintenanceMessage by backupStatus.collectAsState()
            val currentOperator by operatorId.collectAsState()
            val currentPin by operatorPin.collectAsState()
            val diagnostics = SystemDiagnosticsEvaluator.evaluate(appSettings.deviceId, endpoint.path.name, devices.size, appSettings.localCertificateSha256, snapshot, events.size)
            MyfistTheme {
                MyfistBackground {
                    val provisioningActive = provisioningState.phase in setOf(ProvisioningPhase.CONNECTING_SETUP_AP, ProvisioningPhase.AUTHORIZING, ProvisioningPhase.APPLYING, ProvisioningPhase.WAITING_FOR_RESTART, ProvisioningPhase.DISCOVERING_LOCAL)
                    if (appSettings.deviceId.isBlank() || provisioningActive) {
                        ProvisioningScreen(
                            state = provisioningState,
                            wifiNetworks = wifiNetworks,
                            onScan = ::requestQrScan,
                            onScanWifi = provisioning::scanWifi,
                            onProvision = provisioning::provision,
                            onCloudAttach = ::attachExistingCloudDevice,
                        )
                    } else {
                        DashboardScreen(
                            versionName = BuildConfig.VERSION_NAME,
                            localDevices = devices.size,
                            route = endpoint.path.name,
                            cloudStatus = cloudStatus,
                            cloudProfile = cloudProfile,
                            deviceId = appSettings.deviceId,
                            snapshot = snapshot,
                            events = events,
                            diagnostics = diagnostics,
                            backupStatus = maintenanceMessage,
                            commandStatus = commandMessage,
                            operatorId = currentOperator,
                            operatorPin = currentPin,
                            criticalNotificationsEnabled = appSettings.criticalNotificationsEnabled,
                            statusNotificationsEnabled = appSettings.statusNotificationsEnabled,
                            zoneNotificationsEnabled = appSettings.zoneNotificationsEnabled,
                            onOperatorIdChange = { operatorId.value = it.take(23) },
                            onOperatorPinChange = { operatorPin.value = it },
                            onLogin = ::loginCloudProfile,
                            onCriticalNotificationsChange = { enabled -> lifecycleScope.launch { settings.update(settings.settings.value.copy(criticalNotificationsEnabled = enabled)) } },
                            onStatusNotificationsChange = { enabled -> lifecycleScope.launch { settings.update(settings.settings.value.copy(statusNotificationsEnabled = enabled)) } },
                            onZoneNotificationsChange = { enabled -> lifecycleScope.launch { settings.update(settings.settings.value.copy(zoneNotificationsEnabled = enabled)) } },
                            onClearEventHistory = { eventHistory.clear(); telemetry.clearEvents() },
                            onExportEvents = { pendingExportText = EventLogExporter.toCsv(events); exportLauncher.launch(EventLogExporter.suggestedFileName()) },
                            onShareEvents = { val payload = EventLogExporter.toCsv(events); startActivity(Intent.createChooser(Intent(Intent.ACTION_SEND).apply { type = "text/csv"; putExtra(Intent.EXTRA_SUBJECT, "Myfist event log"); putExtra(Intent.EXTRA_TEXT, payload) }, "Поділитися журналом")) },
                            onExportSettings = { pendingSettingsBackupText = SettingsBackupCodec.encode(appSettings); settingsBackupLauncher.launch(SettingsBackupCodec.suggestedFileName()) },
                            onImportSettings = { settingsRestoreLauncher.launch("application/json") },
                            onCommand = ::executeCommand,
                        )
                    }
                }
            }
        }
    }

    private fun attachExistingCloudDevice(deviceId: String, actor: String, pin: String) {
        if (!deviceId.startsWith("HG-") || actor.isBlank() || pin.length !in 4..12) { commandStatus.value = "Перевірте Device ID, користувача та PIN"; return }
        operatorId.value = actor
        operatorPin.value = pin
        lifecycleScope.launch {
            settings.update(settings.settings.value.copy(
                deviceId = deviceId,
                remoteAccessEnabled = true,
                cloudBaseUrl = "mqtts://test.mosquitto.org:8886",
                lastKnownLocalUrl = "",
            ))
            commandStatus.value = "Хмарний пристрій підключається…"
        }
    }

    private fun loginCloudProfile() {
        val actor = operatorId.value.trim()
        val pin = operatorPin.value
        if (actor.isBlank() || pin.length !in 4..12) {
            commandStatus.value = "Введіть ID користувача та PIN 4–12 цифр"
            return
        }
        lifecycleScope.launch {
            commandStatus.value = "Перевірка профілю…"
            val profile = cloudState.loginProfile(actor, pin)
            commandStatus.value = if (profile.authenticated) {
                "Вхід: ${profile.name.ifBlank { profile.id }} · ${profile.role.name}"
            } else {
                "Вхід відхилено"
            }
        }
    }

    private fun executeCommand(type: CommandType) {
        val actor = operatorId.value.trim(); val credential = operatorPin.value
        if (actor.isBlank() || credential.length !in 4..12) { commandStatus.value = "Введіть ID оператора та PIN"; return }
        lifecycleScope.launch {
            commandStatus.value = "Виконується: ${type.name}…"
            val result = runCatching { commands.execute(type, actor, credential) }
            commandStatus.value = result.fold(onSuccess = { reply -> if (reply.accepted || reply.duplicate) "OK: ${reply.code}" else "Відхилено: ${reply.code}" }, onFailure = { error -> "Помилка: ${error.message ?: "network"}" })
        }
    }
    private fun requestNotificationPermission() { if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU && ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) notificationPermissionLauncher.launch(Manifest.permission.POST_NOTIFICATIONS) }
    private fun requestQrScan() { val missing = requiredProvisioningPermissions().filter { ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED }; if (missing.isEmpty()) launchQrScanner() else permissionLauncher.launch(missing.toTypedArray()) }
    private fun launchQrScanner() { qrScanner.launch(ScanOptions().setPrompt("Скануйте QR HomeGuard-S3").setBeepEnabled(false).setOrientationLocked(false)) }
    private fun requiredProvisioningPermissions(): List<String> = buildList { add(Manifest.permission.CAMERA); if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) add(Manifest.permission.NEARBY_WIFI_DEVICES) else add(Manifest.permission.ACCESS_FINE_LOCATION) }
    override fun onDestroy() { operatorPin.value = ""; session.stop(); discovery.stop(); super.onDestroy() }
}
