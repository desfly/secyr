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
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.launch
import ua.homeguard.s3.control.CommandController
import ua.homeguard.s3.diagnostics.SystemDiagnosticsEvaluator
import ua.homeguard.s3.events.EventLogExporter
import ua.homeguard.s3.model.AccessLifecycleState
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
import ua.homeguard.s3.ui.screens.AccessGateScreen
import ua.homeguard.s3.ui.screens.AddDeviceScreen
import ua.homeguard.s3.ui.screens.DashboardScreen
import ua.homeguard.s3.ui.screens.DeviceListScreen
import ua.homeguard.s3.ui.screens.ProvisioningScreen
import ua.homeguard.s3.ui.screens.SetupWifiChoice

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
    private val accessLifecycle = MutableStateFlow(AccessLifecycleState.UNAVAILABLE)
    private val accessGateBusy = MutableStateFlow(false)
    private val accessGateMessage = MutableStateFlow("")
    private val setupWifiNetworks = MutableStateFlow<List<SetupWifiChoice>>(emptyList())
    private val addDeviceOpen = MutableStateFlow(false)
    private val provisioningOpen = MutableStateFlow(false)
    private val deviceListOpen = MutableStateFlow(true)
    private var pendingExportText: String = ""
    private var pendingSettingsBackupText: String = ""

    private val exportLauncher = registerForActivityResult(ActivityResultContracts.CreateDocument("text/csv")) { uri ->
        if (uri != null && pendingExportText.isNotEmpty()) runCatching {
            contentResolver.openOutputStream(uri)?.bufferedWriter(Charsets.UTF_8)?.use { it.write(pendingExportText) }
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

    private val qrScanner = registerForActivityResult(ScanContract()) { result -> result.contents?.let(provisioning::acceptQr) }
    private val permissionLauncher = registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {
        if (requiredProvisioningPermissions().all { permission ->
                ContextCompat.checkSelfPermission(this, permission) == PackageManager.PERMISSION_GRANTED
            }) launchQrScanner()
    }
    private val localNetworkPermissionLauncher = registerForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
        if (granted && ::discovery.isInitialized) lifecycleScope.launch { discovery.rescan() }
        requestNotificationPermission()
    }
    private val notificationPermissionLauncher = registerForActivityResult(ActivityResultContracts.RequestPermission()) { }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        settings = SettingsStore(this)
        registeredDevices = RegisteredDeviceStore(this)
        eventHistory = EventHistoryStore(this)
        discovery = LocalDiscoveryCoordinator(this, lifecycleScope)
        resolver = DeviceEndpointResolver(settings, discovery, lifecycleScope)
        provisioning = ProvisioningCoordinator(this, settings, discovery, lifecycleScope)
        telemetry = TelemetrySocket().apply { seedEvents(eventHistory.load()) }
        session = DeviceSession(lifecycleScope, resolver.endpoint, settings, telemetry)
        commands = CommandController(resolver.endpoint, settings)
        notifications = HomeGuardNotifications(this)
        notifications.createChannels()
        requestLocalNetworkPermission()

        lifecycleScope.launch {
            telemetry.liveEvents().collect { event ->
                eventHistory.append(event)
                notifications.notify(event, settings.settings.value)
            }
        }

        lifecycleScope.launch {
            discovery.devices.collect { found ->
                found.forEach { device ->
                    val manual = registeredDevices.devices.value.firstOrNull { candidate ->
                        candidate.deviceId.startsWith("manual-", ignoreCase = true) &&
                            ControllerIdentity.sameController(
                                candidate.deviceId,
                                candidate.baseUrl,
                                device.deviceId,
                                device.baseUrl,
                            )
                    }
                    if (manual != null) {
                        if (registeredDevices.reconcileManual(manual.deviceId, device) &&
                            settings.settings.value.deviceId.equals(manual.deviceId, ignoreCase = true)) {
                            settings.remember(device)
                        }
                    } else {
                        registeredDevices.refreshDiscovered(device)
                    }
                }
            }
        }
        discovery.start()
        session.start()

        setContent {
            val appSettings by settings.settings.collectAsState()
            val devices by discovery.devices.collectAsState()
            val registered by registeredDevices.devices.collectAsState()
            val isScanning by discovery.isScanning.collectAsState()
            val scanStatus by discovery.scanStatus.collectAsState()
            val endpoint by resolver.endpoint.collectAsState()
            val provisioningState by provisioning.state.collectAsState()
            val snapshot by telemetry.snapshots().collectAsState(initial = SystemSnapshot())
            val events by telemetry.events().collectAsState(initial = emptyList())
            val commandMessage by commandStatus.collectAsState()
            val maintenanceMessage by backupStatus.collectAsState()
            val currentOperator by operatorId.collectAsState()
            val currentPin by operatorPin.collectAsState()
            val currentAccessSession by accessSession.collectAsState()
            val lifecycleState by accessLifecycle.collectAsState()
            val gateBusy by accessGateBusy.collectAsState()
            val gateMessage by accessGateMessage.collectAsState()
            val gateNetworks by setupWifiNetworks.collectAsState()
            val showAddDevice by addDeviceOpen.collectAsState()
            val showProvisioning by provisioningOpen.collectAsState()
            val showDeviceList by deviceListOpen.collectAsState()
            val diagnostics = SystemDiagnosticsEvaluator.evaluate(
                appSettings.deviceId,
                endpoint.path.name,
                devices.size,
                appSettings.localCertificateSha256,
                snapshot,
                events.size,
                scanStatus.phase,
                scanStatus.network,
                scanStatus.targets,
                scanStatus.sent,
                scanStatus.received,
                scanStatus.accepted,
                scanStatus.lastResponder,
                scanStatus.error,
            )

            MaterialTheme {
                val provisioningActive = provisioningState.phase in setOf(
                    ProvisioningPhase.CONNECTING_SETUP_AP,
                    ProvisioningPhase.AUTHORIZING,
                    ProvisioningPhase.APPLYING,
                    ProvisioningPhase.WAITING_FOR_RESTART,
                    ProvisioningPhase.DISCOVERING_LOCAL,
                )
                when {
                    showProvisioning || provisioningActive -> ProvisioningScreen(
                        provisioningState,
                        { if (!provisioningActive) { provisioningOpen.value = false; addDeviceOpen.value = true } },
                        ::requestQrScan,
                        provisioning::provision,
                    )

                    showAddDevice -> AddDeviceScreen(
                        devices,
                        isScanning,
                        scanStatus,
                        { addDeviceOpen.value = false; deviceListOpen.value = true },
                        { lifecycleScope.launch { discovery.rescan() } },
                        { device, name ->
                            lifecycleScope.launch {
                                registeredDevices.addOrUpdate(device, name)
                                settings.remember(device)
                                addDeviceOpen.value = false
                                deviceListOpen.value = true
                            }
                        },
                        { name, address -> addManualDevice(name, address) },
                        { name, deviceId -> addManualDeviceId(name, deviceId) },
                        { addDeviceOpen.value = false; provisioningOpen.value = true },
                    )

                    showDeviceList -> DeviceListScreen(
                        devices = registered,
                        discovered = devices,
                        activeDeviceId = appSettings.deviceId,
                        snapshot = snapshot,
                        onAddDevice = { addDeviceOpen.value = true; lifecycleScope.launch { discovery.rescan() } },
                        onRenameDevice = { device, newName -> lifecycleScope.launch { registeredDevices.rename(device.deviceId, newName) } },
                        onDeleteDevice = { device ->
                            lifecycleScope.launch {
                                registeredDevices.remove(device.deviceId)
                                if (settings.settings.value.deviceId == device.deviceId) {
                                    commands.logout()
                                    settings.selectDevice("")
                                    accessSession.value = null
                                    accessLifecycle.value = AccessLifecycleState.UNAVAILABLE
                                    operatorPin.value = ""
                                }
                            }
                        },
                        onOpenDevice = { device -> openController(device.deviceId, device.baseUrl.takeIf { it.isNotBlank() }) },
                    )

                    currentAccessSession == null -> AccessGateScreen(
                        state = lifecycleState,
                        busy = gateBusy,
                        message = gateMessage,
                        wifiNetworks = gateNetworks,
                        onRetry = ::refreshAccessLifecycle,
                        onScanWifi = ::scanSetupWifi,
                        onConnectWifi = ::connectSetupWifi,
                        onBootstrapAdmin = ::bootstrapFirstAdmin,
                        onLogin = ::loginFromGate,
                        onBack = {
                            commands.logout()
                            accessSession.value = null
                            operatorPin.value = ""
                            accessLifecycle.value = AccessLifecycleState.UNAVAILABLE
                            deviceListOpen.value = true
                        },
                    )

                    else -> DashboardScreen(
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
                        onBackToDevices = {
                            logoutOperator()
                            deviceListOpen.value = true
                        },
                        onAddDevice = {
                            logoutOperator()
                            deviceListOpen.value = true
                            addDeviceOpen.value = true
                            lifecycleScope.launch { discovery.rescan() }
                        },
                        onOperatorIdChange = { value -> operatorId.value = value.take(23) },
                        onOperatorPinChange = { value -> operatorPin.value = value.filter(Char::isDigit).take(12) },
                        onLogin = ::loginOperator,
                        onLogout = ::logoutOperator,
                        onCriticalNotificationsChange = { enabled -> lifecycleScope.launch { settings.update(settings.settings.value.copy(criticalNotificationsEnabled = enabled)) } },
                        onStatusNotificationsChange = { enabled -> lifecycleScope.launch { settings.update(settings.settings.value.copy(statusNotificationsEnabled = enabled)) } },
                        onZoneNotificationsChange = { enabled -> lifecycleScope.launch { settings.update(settings.settings.value.copy(zoneNotificationsEnabled = enabled)) } },
                        onClearEventHistory = { eventHistory.clear(); telemetry.clearEvents() },
                        onExportEvents = { pendingExportText = EventLogExporter.toCsv(events); exportLauncher.launch(EventLogExporter.suggestedFileName()) },
                        onShareEvents = {
                            val payload = EventLogExporter.toCsv(events)
                            startActivity(Intent.createChooser(Intent(Intent.ACTION_SEND).apply {
                                type = "text/csv"
                                putExtra(Intent.EXTRA_SUBJECT, "HomeGuard-S3 event log")
                                putExtra(Intent.EXTRA_TEXT, payload)
                            }, "Поділитися журналом"))
                        },
                        onExportSettings = { pendingSettingsBackupText = SettingsBackupCodec.encode(appSettings); settingsBackupLauncher.launch(SettingsBackupCodec.suggestedFileName()) },
                        onImportSettings = { settingsRestoreLauncher.launch("application/json") },
                        onFactoryReset = ::factoryResetController,
                        onCommand = ::executeCommand,
                    )
                }
            }
        }
    }

    private fun openController(deviceId: String, baseUrl: String?) {
        lifecycleScope.launch {
            commands.logout()
            accessSession.value = null
            operatorPin.value = ""
            accessLifecycle.value = AccessLifecycleState.UNAVAILABLE
            accessGateMessage.value = "Перевірка стану доступу…"
            setupWifiNetworks.value = emptyList()
            settings.selectDevice(deviceId, baseUrl)
            deviceListOpen.value = false
            // Resolver is flow-based; allow the newly selected local endpoint to propagate.
            delay(250)
            refreshAccessLifecycle()
        }
    }

    private fun refreshAccessLifecycle() {
        lifecycleScope.launch {
            accessGateBusy.value = true
            accessGateMessage.value = "Перевірка стану доступу…"
            commands.logout()
            accessSession.value = null
            operatorPin.value = ""
            runCatching { commands.accessState() }
                .onSuccess { state ->
                    accessLifecycle.value = state
                    accessGateMessage.value = when (state) {
                        AccessLifecycleState.SETUP_REQUIRED -> "Первинний setup відкритий до успішного створення першого Admin."
                        AccessLifecycleState.LOGIN_REQUIRED -> "Контролер захищений. Увійдіть."
                        AccessLifecycleState.UNAVAILABLE -> "Контролер не повернув валідний стан доступу."
                    }
                }
                .onFailure { error ->
                    accessLifecycle.value = AccessLifecycleState.UNAVAILABLE
                    accessGateMessage.value = "Стан доступу недоступний: ${error.message ?: "network"}"
                }
            accessGateBusy.value = false
        }
    }

    private fun scanSetupWifi() {
        lifecycleScope.launch {
            accessGateBusy.value = true
            accessGateMessage.value = "Сканування Wi-Fi…"
            runCatching { commands.setupWifiScan() }
                .onSuccess { json ->
                    val array = json.optJSONArray("networks")
                    val networks = buildList {
                        if (array != null) for (i in 0 until array.length()) {
                            val item = array.optJSONObject(i) ?: continue
                            add(SetupWifiChoice(item.optString("ssid", ""), item.optInt("rssi", 0)))
                        }
                    }
                    setupWifiNetworks.value = networks
                    accessGateMessage.value = "Знайдено Wi-Fi мереж: ${networks.size}"
                }
                .onFailure { error -> accessGateMessage.value = "Помилка сканування Wi-Fi: ${error.message ?: "network"}" }
            accessGateBusy.value = false
        }
    }

    private fun connectSetupWifi(ssid: String, password: String) {
        lifecycleScope.launch {
            accessGateBusy.value = true
            accessGateMessage.value = "Підключення до $ssid…"
            runCatching { commands.setupConfigureWifi(ssid, password) }
                .onSuccess { accessGateMessage.value = "Wi-Fi $ssid збережено. Setup лишається відкритим до створення Admin." }
                .onFailure { error -> accessGateMessage.value = "Помилка Wi-Fi: ${error.message ?: "network"}" }
            accessGateBusy.value = false
        }
    }

    private fun bootstrapFirstAdmin(id: String, name: String, pin: String) {
        lifecycleScope.launch {
            accessGateBusy.value = true
            accessGateMessage.value = "Створення першого Admin…"
            runCatching { commands.bootstrapAdmin(id, name, pin) }
                .onSuccess {
                    operatorId.value = id
                    operatorPin.value = ""
                    accessLifecycle.value = AccessLifecycleState.LOGIN_REQUIRED
                    setupWifiNetworks.value = emptyList()
                    accessGateMessage.value = "Admin створений. Безпарольний setup закрито. Увійдіть новим PIN."
                }
                .onFailure { error ->
                    // Failed bootstrap must not consume setup. Confirm state again.
                    accessGateMessage.value = "Admin не створений: ${error.message ?: "network"}. Setup залишається доступним."
                    runCatching { commands.accessState() }.onSuccess { accessLifecycle.value = it }
                }
            accessGateBusy.value = false
        }
    }

    private fun loginFromGate(actor: String, pin: String) {
        operatorId.value = actor
        operatorPin.value = pin
        loginOperator()
    }

    private fun addManualDevice(name: String, rawAddress: String) {
        val baseUrl = DiscoveryInputValidator.normalizeManualAddress(rawAddress) ?: run {
            commandStatus.value = "Некоректна адреса. Введіть IP або IP:порт"
            return
        }
        val deviceId = "manual-${baseUrl.lowercase().hashCode().toUInt().toString(16)}"
        lifecycleScope.launch {
            registeredDevices.addManual(deviceId, baseUrl, name)
            settings.selectDevice(deviceId, baseUrl)
            addDeviceOpen.value = false
            deviceListOpen.value = true
        }
    }

    private fun addManualDeviceId(name: String, rawDeviceId: String) {
        val deviceId = DiscoveryInputValidator.normalizeDeviceId(rawDeviceId) ?: run {
            commandStatus.value = "Некоректний ID пристрою"
            return
        }
        lifecycleScope.launch {
            registeredDevices.addManual(deviceId, "", name)
            settings.selectDevice(deviceId)
            addDeviceOpen.value = false
            deviceListOpen.value = true
            discovery.rescan()
        }
    }

    private fun loginOperator() {
        val actor = operatorId.value.trim()
        val credential = operatorPin.value
        if (actor.isBlank() || credential.length !in 4..12 || !credential.all(Char::isDigit)) {
            commandStatus.value = "Введіть ID користувача та PIN 4–12 цифр"
            accessGateMessage.value = commandStatus.value
            accessSession.value = null
            return
        }
        lifecycleScope.launch {
            accessGateBusy.value = true
            commandStatus.value = "Перевірка доступу…"
            accessGateMessage.value = commandStatus.value
            runCatching { commands.login(actor, credential) }
                .onSuccess { authenticated ->
                    accessSession.value = authenticated
                    accessLifecycle.value = AccessLifecycleState.LOGIN_REQUIRED
                    commandStatus.value = "Вхід: ${authenticated.name} · ${authenticated.role.name.lowercase()}"
                    accessGateMessage.value = ""
                }
                .onFailure { error ->
                    commands.logout()
                    accessSession.value = null
                    commandStatus.value = "Вхід відхилено: ${error.message ?: "network"}"
                    accessGateMessage.value = commandStatus.value
                }
            accessGateBusy.value = false
        }
    }

    private fun logoutOperator() {
        commands.logout()
        accessSession.value = null
        operatorPin.value = ""
        accessLifecycle.value = AccessLifecycleState.LOGIN_REQUIRED
        commandStatus.value = "Сеанс завершено"
        accessGateMessage.value = "Сеанс завершено. Увійдіть знову."
    }

    private fun factoryResetController() {
        val authenticated = accessSession.value
        val actor = operatorId.value.trim()
        val credential = operatorPin.value
        if (authenticated == null || authenticated.actor != actor || authenticated.role.name != "ADMIN") {
            commandStatus.value = "Factory Reset доступний тільки після входу Admin"
            return
        }
        if (credential.length !in 4..12 || !credential.all(Char::isDigit)) {
            logoutOperator()
            commandStatus.value = "PIN сеансу відсутній — увійдіть знову"
            return
        }
        val target = resolver.endpoint.value
        if (target.apiBaseUrl.isBlank() || target.path.name == "OFFLINE" || target.path.name == "CLOUD") {
            commandStatus.value = "Factory Reset потребує локального підключення до контролера"
            return
        }
        val selectedId = settings.settings.value.deviceId
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
                    if (selectedId.isNotBlank()) registeredDevices.markAuthorization(selectedId, false)
                    commands.logout()
                    settings.selectDevice("")
                    accessSession.value = null
                    accessLifecycle.value = AccessLifecycleState.UNAVAILABLE
                    operatorPin.value = ""
                    addDeviceOpen.value = false
                    provisioningOpen.value = false
                    deviceListOpen.value = true
                    commandStatus.value = if (outcome == FactoryResetResult.ACCEPTED) {
                        "Factory Reset прийнято; контролер перезавантажується"
                    } else {
                        "Зв’язок обірвався під час Factory Reset; локальну сесію закрито"
                    }
                }
            }
        }
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
            logoutOperator()
            commandStatus.value = "PIN сеансу відсутній — увійдіть знову"
            return
        }
        lifecycleScope.launch {
            commandStatus.value = "Виконується: ${type.name}…"
            val result = runCatching { commands.execute(type, actor, credential) }
            commandStatus.value = result.fold(
                { reply ->
                    if (reply.accepted || reply.duplicate) "OK: ${reply.code}" else {
                        if (reply.code.contains("unauthorized", true) || reply.code.contains("credential", true) || reply.code.contains("rate", true)) logoutOperator()
                        "Відхилено: ${reply.code}"
                    }
                },
                { error -> "Помилка: ${error.message ?: "network"}" },
            )
        }
    }

    private fun requestLocalNetworkPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
            ContextCompat.checkSelfPermission(this, Manifest.permission.NEARBY_WIFI_DEVICES) != PackageManager.PERMISSION_GRANTED) {
            localNetworkPermissionLauncher.launch(Manifest.permission.NEARBY_WIFI_DEVICES)
            return
        }
        requestNotificationPermission()
    }

    private fun requestNotificationPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
            ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
            notificationPermissionLauncher.launch(Manifest.permission.POST_NOTIFICATIONS)
        }
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
        commands.logout()
        accessSession.value = null
        operatorPin.value = ""
        session.stop()
        discovery.stop()
        super.onDestroy()
    }
}
