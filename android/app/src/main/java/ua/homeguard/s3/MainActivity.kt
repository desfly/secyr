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
import ua.homeguard.s3.network.ControllerIdentity
import ua.homeguard.s3.network.DeviceEndpointResolver
import ua.homeguard.s3.network.DeviceSession
import ua.homeguard.s3.network.DiscoveryInputValidator
import ua.homeguard.s3.network.FactoryResetClient
import ua.homeguard.s3.network.FactoryResetResult
import ua.homeguard.s3.network.LocalDiscoveryCoordinator
import ua.homeguard.s3.network.TelemetrySocket
import ua.homeguard.s3.notifications.HomeGuardNotifications
import ua.homeguard.s3.repository.ProvisioningCoordinator
import ua.homeguard.s3.storage.EventHistoryStore
import ua.homeguard.s3.storage.RegisteredDeviceStore
import ua.homeguard.s3.storage.SettingsBackupCodec
import ua.homeguard.s3.storage.SettingsStore
import ua.homeguard.s3.ui.screens.AddDeviceScreen
import ua.homeguard.s3.ui.screens.DashboardScreen
import ua.homeguard.s3.ui.screens.DeviceListScreen
import ua.homeguard.s3.ui.screens.ProvisioningScreen
import ua.homeguard.s3.ui.screens.eventsForController

class MainActivity : ComponentActivity() {
    private lateinit var discovery: LocalDiscoveryCoordinator
    private lateinit var settings: SettingsStore
    private lateinit var registeredDevices: RegisteredDeviceStore
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
    private val addDeviceOpen = MutableStateFlow(false)
    private val provisioningOpen = MutableStateFlow(false)
    private val deviceListOpen = MutableStateFlow(true)
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
    private val permissionLauncher = registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { if (requiredProvisioningPermissions().all { permission -> ContextCompat.checkSelfPermission(this, permission) == PackageManager.PERMISSION_GRANTED }) launchQrScanner() }
    private val localNetworkPermissionLauncher = registerForActivityResult(ActivityResultContracts.RequestPermission()) { granted -> if (granted && ::discovery.isInitialized) lifecycleScope.launch { discovery.rescan() }; requestNotificationPermission() }
    private val notificationPermissionLauncher = registerForActivityResult(ActivityResultContracts.RequestPermission()) { }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        settings = SettingsStore(this); registeredDevices = RegisteredDeviceStore(this); eventHistory = EventHistoryStore(this)
        discovery = LocalDiscoveryCoordinator(this, lifecycleScope); resolver = DeviceEndpointResolver(settings, discovery, lifecycleScope)
        provisioning = ProvisioningCoordinator(this, settings, discovery, registeredDevices, lifecycleScope)
        telemetry = TelemetrySocket().apply { seedEvents(eventHistory.load()) }
        session = DeviceSession(lifecycleScope, resolver.endpoint, settings, telemetry, registeredDevices)
        commands = CommandController(resolver.endpoint, settings); notifications = HomeGuardNotifications(this); notifications.createChannels(); requestLocalNetworkPermission()
        lifecycleScope.launch { telemetry.liveEvents().collect { event -> eventHistory.append(event); notifications.notify(event, settings.settings.value) } }
        lifecycleScope.launch {
            settings.settings.collect { config ->
                val authenticated = accessSession.value
                if (authenticated != null && !authenticated.belongsTo(config.deviceId)) {
                    accessSession.value = null
                    operatorPin.value = ""
                }
            }
        }
        lifecycleScope.launch {
            discovery.devices.collect { found ->
                found.forEach { device ->
                    val manual = registeredDevices.devices.value.firstOrNull { candidate ->
                        candidate.deviceId.startsWith("manual-", ignoreCase = true) &&
                            ControllerIdentity.sameController(candidate.deviceId, candidate.baseUrl, device.deviceId, device.baseUrl)
                    }
                    if (manual != null) {
                        if (registeredDevices.reconcileManual(manual.deviceId, device) && settings.settings.value.deviceId.equals(manual.deviceId, ignoreCase = true)) settings.remember(device)
                    } else {
                        registeredDevices.refreshDiscovered(device)
                    }
                }
            }
        }
        session.start()

        setContent {
            val appSettings by settings.settings.collectAsState(); val devices by discovery.devices.collectAsState(); val registered by registeredDevices.devices.collectAsState()
            val isScanning by discovery.isScanning.collectAsState(); val scanStatus by discovery.scanStatus.collectAsState(); val endpoint by resolver.endpoint.collectAsState()
            val provisioningState by provisioning.state.collectAsState(); val snapshot by telemetry.snapshots().collectAsState(initial = SystemSnapshot()); val events by telemetry.events().collectAsState(initial = emptyList())
            val commandMessage by commandStatus.collectAsState(); val maintenanceMessage by backupStatus.collectAsState(); val currentOperator by operatorId.collectAsState(); val currentPin by operatorPin.collectAsState(); val currentAccessSession by accessSession.collectAsState()
            val showAddDevice by addDeviceOpen.collectAsState(); val showProvisioning by provisioningOpen.collectAsState(); val showDeviceList by deviceListOpen.collectAsState()
            val selectedEventCount = eventsForController(events, appSettings.deviceId).size
            val diagnostics = SystemDiagnosticsEvaluator.evaluate(appSettings.deviceId, endpoint.path.name, devices.size, appSettings.localCertificateSha256, snapshot, selectedEventCount, scanStatus.phase, scanStatus.network, scanStatus.targets, scanStatus.sent, scanStatus.received, scanStatus.accepted, scanStatus.lastResponder, scanStatus.error)
            MaterialTheme {
                val provisioningActive = provisioningState.phase in setOf(ProvisioningPhase.CONNECTING_SETUP_AP, ProvisioningPhase.AUTHORIZING, ProvisioningPhase.APPLYING, ProvisioningPhase.WAITING_FOR_RESTART, ProvisioningPhase.DISCOVERING_LOCAL)
                when {
                    showProvisioning || provisioningActive -> ProvisioningScreen(
                        state = provisioningState,
                        devices = devices,
                        isScanningNetwork = isScanning,
                        scanStatus = scanStatus,
                        onBack = { if (!provisioningActive) { provisioningOpen.value = false; addDeviceOpen.value = true } },
                        onScanQr = ::requestQrScan,
                        onDiscover = { lifecycleScope.launch { discovery.rescan() } },
                        onUseDevice = { device, name -> lifecycleScope.launch { registeredDevices.addOrUpdate(device, name); settings.remember(device); provisioningOpen.value = false; deviceListOpen.value = true } },
                        onUseManualIp = { address, name -> addManualDevice(name, address) },
                        onProvision = provisioning::provision,
                    )
                    showAddDevice -> AddDeviceScreen(devices, isScanning, scanStatus, { addDeviceOpen.value = false; deviceListOpen.value = true }, { lifecycleScope.launch { discovery.rescan() } }, { device, name -> lifecycleScope.launch { registeredDevices.addOrUpdate(device, name); settings.remember(device); addDeviceOpen.value = false; deviceListOpen.value = true } }, { name, address -> addManualDevice(name, address) }, { name, deviceId -> addManualDeviceId(name, deviceId) }, { addDeviceOpen.value = false; provisioningOpen.value = true })
                    showDeviceList -> DeviceListScreen(devices = registered, discovered = devices, activeDeviceId = appSettings.deviceId, snapshot = snapshot, onAddDevice = { addDeviceOpen.value = true; lifecycleScope.launch { discovery.rescan() } }, onRenameDevice = { device, newName -> lifecycleScope.launch { registeredDevices.rename(device.deviceId, newName) } }, onDeleteDevice = { device -> lifecycleScope.launch { registeredDevices.remove(device.deviceId); if (settings.settings.value.deviceId.equals(device.deviceId, ignoreCase = true)) { settings.selectDevice(""); accessSession.value = null; operatorPin.value = "" } } }, onOpenDevice = { device -> lifecycleScope.launch { settings.selectDevice(device.deviceId, device.baseUrl.takeIf { it.isNotBlank() }); deviceListOpen.value = false } })
                    else -> DashboardScreen(versionName = BuildConfig.VERSION_NAME, localDevices = devices.size, route = endpoint.path.name, deviceId = appSettings.deviceId, snapshot = snapshot, events = events, diagnostics = diagnostics, backupStatus = maintenanceMessage, commandStatus = commandMessage, operatorId = currentOperator, operatorPin = currentPin, accessSession = currentAccessSession, criticalNotificationsEnabled = appSettings.criticalNotificationsEnabled, statusNotificationsEnabled = appSettings.statusNotificationsEnabled, zoneNotificationsEnabled = appSettings.zoneNotificationsEnabled, onBackToDevices = { deviceListOpen.value = true }, onAddDevice = { deviceListOpen.value = true; addDeviceOpen.value = true; lifecycleScope.launch { discovery.rescan() } }, onOperatorIdChange = { value -> operatorId.value = value.take(23); accessSession.value = null }, onOperatorPinChange = { value -> operatorPin.value = value.filter(Char::isDigit).take(12); accessSession.value = null }, onLogin = ::loginOperator, onLogout = ::logoutOperator, onCriticalNotificationsChange = { enabled -> lifecycleScope.launch { settings.update(settings.settings.value.copy(criticalNotificationsEnabled = enabled)) } }, onStatusNotificationsChange = { enabled -> lifecycleScope.launch { settings.update(settings.settings.value.copy(statusNotificationsEnabled = enabled)) } }, onZoneNotificationsChange = { enabled -> lifecycleScope.launch { settings.update(settings.settings.value.copy(zoneNotificationsEnabled = enabled)) } }, onClearEventHistory = { eventHistory.clear(); telemetry.clearEvents() }, onExportEvents = { pendingExportText = EventLogExporter.toCsv(events); exportLauncher.launch(EventLogExporter.suggestedFileName()) }, onShareEvents = { val payload = EventLogExporter.toCsv(events); startActivity(Intent.createChooser(Intent(Intent.ACTION_SEND).apply { type = "text/csv"; putExtra(Intent.EXTRA_SUBJECT, "HomeGuard-S3 event log"); putExtra(Intent.EXTRA_TEXT, payload) }, "Поділитися журналом")) }, onExportSettings = { pendingSettingsBackupText = SettingsBackupCodec.encode(appSettings); settingsBackupLauncher.launch("application/json") }, onImportSettings = { settingsRestoreLauncher.launch("application/json") }, onFactoryReset = ::factoryResetController, onCommand = ::executeCommand)
                }
            }
        }
    }

    override fun onStart() {
        super.onStart()
        discovery.start()
    }

    override fun onStop() {
        discovery.stop()
        super.onStop()
    }

    private fun addManualDevice(name: String, rawAddress: String) {
        val baseUrl = DiscoveryInputValidator.normalizeManualAddress(rawAddress) ?: run { commandStatus.value = "Некоректна адреса. Введіть IP або IP:порт"; return }
        val deviceId = "manual-${baseUrl.lowercase().hashCode().toUInt().toString(16)}"
        lifecycleScope.launch { registeredDevices.addManual(deviceId, baseUrl, name); settings.selectDevice(deviceId, baseUrl); addDeviceOpen.value = false; provisioningOpen.value = false; deviceListOpen.value = true }
    }
    private fun addManualDeviceId(name: String, rawDeviceId: String) {
        val deviceId = DiscoveryInputValidator.normalizeDeviceId(rawDeviceId) ?: run { commandStatus.value = "Некоректний ID пристрою"; return }
        lifecycleScope.launch { registeredDevices.addManual(deviceId, "", name); settings.selectDevice(deviceId); addDeviceOpen.value = false; deviceListOpen.value = true; discovery.rescan() }
    }
    private fun loginOperator() {
        val actor = operatorId.value.trim(); val credential = operatorPin.value
        if (actor.isBlank() || credential.length !in 4..12 || !credential.all(Char::isDigit)) { commandStatus.value = "Введіть ID користувача та PIN 4–12 цифр"; accessSession.value = null; return }
        lifecycleScope.launch { commandStatus.value = "Перевірка доступу…"; runCatching { commands.login(actor, credential) }.onSuccess { authenticated -> accessSession.value = authenticated; commandStatus.value = "Вхід: ${authenticated.name} · ${authenticated.role.name.lowercase()}" }.onFailure { error -> accessSession.value = null; commandStatus.value = "Вхід відхилено: ${error.message ?: "network"}" } }
    }
    private fun logoutOperator() { accessSession.value = null; operatorPin.value = ""; commandStatus.value = "Сеанс завершено" }

    private fun factoryResetController() {
        val authenticated = accessSession.value
        val actor = operatorId.value.trim()
        val credential = operatorPin.value
        val selectedDeviceId = settings.settings.value.deviceId
        if (authenticated == null || authenticated.actor != actor || authenticated.role.name != "ADMIN" || !authenticated.belongsTo(selectedDeviceId)) {
            commandStatus.value = "Factory Reset доступний тільки після входу Admin на цьому контролері"
            return
        }
        if (credential.length !in 4..12 || !credential.all(Char::isDigit)) {
            accessSession.value = null
            commandStatus.value = "PIN сеансу відсутній — увійдіть знову"
            return
        }
        val target = resolver.endpoint.value
        if (target.apiBaseUrl.isBlank() || target.path.name == "OFFLINE" || target.path.name == "CLOUD" || !authenticated.belongsTo(target.deviceId)) {
            commandStatus.value = "Factory Reset потребує локального підключення до авторизованого контролера"
            return
        }
        val resetDeviceId = target.deviceId
        lifecycleScope.launch {
            commandStatus.value = "Factory Reset…"
            val outcome = runCatching {
                FactoryResetClient(target.apiBaseUrl, target.certificateSha256).reset(actor, credential)
            }.getOrElse { error ->
                commandStatus.value = "Factory Reset: ${error.message ?: "помилка"}"
                return@launch
            }
            when (outcome) {
                FactoryResetResult.REJECTED -> commandStatus.value = "Factory Reset відхилено контролером"
                FactoryResetResult.ACCEPTED,
                FactoryResetResult.CONNECTION_LOST -> {
                    if (resetDeviceId.isNotBlank()) registeredDevices.markAuthorization(resetDeviceId, false)
                    val stillSelected = settings.settings.value.deviceId.equals(resetDeviceId, ignoreCase = true)
                    if (stillSelected) {
                        settings.selectDevice("")
                        accessSession.value = null
                        operatorPin.value = ""
                        addDeviceOpen.value = false
                        provisioningOpen.value = false
                        deviceListOpen.value = true
                    }
                    commandStatus.value = when {
                        !stillSelected -> "Factory Reset попереднього контролера завершено; поточний вибір не змінено"
                        outcome == FactoryResetResult.ACCEPTED -> "Factory Reset прийнято; контролер перезавантажується"
                        else -> "Зв’язок обірвався під час Factory Reset; локальну сесію закрито"
                    }
                }
            }
        }
    }

    private fun executeCommand(type: CommandType) {
        val actor = operatorId.value.trim(); val credential = operatorPin.value; val authenticated = accessSession.value
        val selectedDeviceId = settings.settings.value.deviceId
        if (authenticated == null || authenticated.actor != actor || !authenticated.belongsTo(selectedDeviceId)) { commandStatus.value = "Спочатку увійдіть на цьому контролері"; return }
        if (!authenticated.allows(type)) { commandStatus.value = "Недоступно для ролі ${authenticated.role.name.lowercase()}"; return }
        if (credential.length !in 4..12 || !credential.all(Char::isDigit)) { accessSession.value = null; commandStatus.value = "PIN сеансу відсутній — увійдіть знову"; return }
        lifecycleScope.launch { commandStatus.value = "Виконується: ${type.name}…"; val result = runCatching { commands.execute(type, actor, credential) }; commandStatus.value = result.fold({ reply -> if (reply.accepted || reply.duplicate) "OK: ${reply.code}" else { if (reply.code.contains("unauthorized", true) || reply.code.contains("credential", true) || reply.code.contains("rate", true)) accessSession.value = null; "Відхилено: ${reply.code}" } }, { error -> "Помилка: ${error.message ?: "network"}" }) }
    }
    private fun requestLocalNetworkPermission() { if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU && ContextCompat.checkSelfPermission(this, Manifest.permission.NEARBY_WIFI_DEVICES) != PackageManager.PERMISSION_GRANTED) { localNetworkPermissionLauncher.launch(Manifest.permission.NEARBY_WIFI_DEVICES); return }; requestNotificationPermission() }
    private fun requestNotificationPermission() { if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU && ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) notificationPermissionLauncher.launch(Manifest.permission.POST_NOTIFICATIONS) }
    private fun requestQrScan() { val missing = requiredProvisioningPermissions().filter { ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED }; if (missing.isEmpty()) launchQrScanner() else permissionLauncher.launch(missing.toTypedArray()) }
    private fun launchQrScanner() { qrScanner.launch(ScanOptions().setPrompt("Скануйте QR HomeGuard-S3").setBeepEnabled(false).setOrientationLocked(false)) }
    private fun requiredProvisioningPermissions(): List<String> = buildList { add(Manifest.permission.CAMERA); if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) add(Manifest.permission.NEARBY_WIFI_DEVICES) else add(Manifest.permission.ACCESS_FINE_LOCATION) }
    override fun onDestroy() { accessSession.value = null; operatorPin.value = ""; session.stop(); discovery.stop(); super.onDestroy() }
}
