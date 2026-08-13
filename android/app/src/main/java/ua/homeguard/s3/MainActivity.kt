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
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.launch
import ua.homeguard.s3.control.CommandController
import ua.homeguard.s3.diagnostics.SystemDiagnosticsEvaluator
import ua.homeguard.s3.events.EventLogExporter
import ua.homeguard.s3.model.AccessSession
import ua.homeguard.s3.model.CommandType
import ua.homeguard.s3.model.RegisteredDevice
import ua.homeguard.s3.model.SystemSnapshot
import ua.homeguard.s3.network.DeviceEndpointResolver
import ua.homeguard.s3.network.DeviceSession
import ua.homeguard.s3.network.LocalDiscoveryCoordinator
import ua.homeguard.s3.network.TelemetrySocket
import ua.homeguard.s3.notifications.HomeGuardNotifications
import ua.homeguard.s3.repository.DeviceAddResult
import ua.homeguard.s3.repository.DeviceRegistryCoordinator
import ua.homeguard.s3.storage.EventHistoryStore
import ua.homeguard.s3.storage.RegisteredDeviceStore
import ua.homeguard.s3.storage.SettingsBackupCodec
import ua.homeguard.s3.storage.SettingsStore
import ua.homeguard.s3.storage.UserCredentialStore
import ua.homeguard.s3.ui.screens.AddDeviceScreen
import ua.homeguard.s3.ui.screens.DashboardScreen
import ua.homeguard.s3.ui.screens.FirstRunLoginScreen
import ua.homeguard.s3.ui.screens.RegisteredDevicesScreen

private enum class RootPage { DEVICES, ADD_DEVICE, MONITOR }

class MainActivity : ComponentActivity() {
    private lateinit var discovery: LocalDiscoveryCoordinator
    private lateinit var settings: SettingsStore
    private lateinit var credentials: UserCredentialStore
    private lateinit var registeredDevices: RegisteredDeviceStore
    private lateinit var registryCoordinator: DeviceRegistryCoordinator
    private lateinit var eventHistory: EventHistoryStore
    private lateinit var resolver: DeviceEndpointResolver
    private lateinit var telemetry: TelemetrySocket
    private lateinit var session: DeviceSession
    private lateinit var commands: CommandController
    private lateinit var notifications: HomeGuardNotifications

    private val commandStatus = MutableStateFlow("Готово")
    private val backupStatus = MutableStateFlow("Backup/restore готовий")
    private val enrollmentStatus = MutableStateFlow("")
    private val enrollmentBusy = MutableStateFlow(false)
    private val operatorId = MutableStateFlow("")
    private val operatorPassword = MutableStateFlow("")
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
                backupStatus.value = "Restore виконано"
            }.onFailure { error ->
                backupStatus.value = "Помилка restore: ${error.message ?: "invalid backup"}"
            }
        }
    }

    private val notificationPermissionLauncher = registerForActivityResult(ActivityResultContracts.RequestPermission()) { }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        settings = SettingsStore(this)
        credentials = UserCredentialStore(this)
        registeredDevices = RegisteredDeviceStore(this)
        eventHistory = EventHistoryStore(this)
        discovery = LocalDiscoveryCoordinator(this, lifecycleScope)
        registryCoordinator = DeviceRegistryCoordinator(lifecycleScope, credentials, registeredDevices)
        resolver = DeviceEndpointResolver(settings, discovery, lifecycleScope)
        telemetry = TelemetrySocket().apply { seedEvents(eventHistory.load()) }
        session = DeviceSession(lifecycleScope, resolver.endpoint, settings, telemetry)
        commands = CommandController(resolver.endpoint, settings)
        notifications = HomeGuardNotifications(this)
        notifications.createChannels()
        requestNotificationPermission()

        credentials.credentials.value?.let {
            operatorId.value = it.username
            operatorPassword.value = it.password
        }

        lifecycleScope.launch {
            telemetry.liveEvents().collect { event ->
                eventHistory.append(event)
                notifications.notify(event, settings.settings.value)
            }
        }
        lifecycleScope.launch {
            discovery.devices.collect { found -> registryCoordinator.refresh(found) }
        }

        discovery.start()
        session.start()

        setContent {
            val appSettings by settings.settings.collectAsState()
            val foundDevices by discovery.devices.collectAsState()
            val savedCredentials by credentials.credentials.collectAsState()
            val devices by registeredDevices.devices.collectAsState()
            val endpoint by resolver.endpoint.collectAsState()
            val snapshot by telemetry.snapshots().collectAsState(initial = SystemSnapshot())
            val events by telemetry.events().collectAsState(initial = emptyList())
            val commandMessage by commandStatus.collectAsState()
            val maintenanceMessage by backupStatus.collectAsState()
            val addMessage by enrollmentStatus.collectAsState()
            val addBusy by enrollmentBusy.collectAsState()
            val currentOperator by operatorId.collectAsState()
            val currentPassword by operatorPassword.collectAsState()
            val currentAccessSession by accessSession.collectAsState()

            var rootPage by remember { mutableStateOf(RootPage.DEVICES) }
            var selectedDevice by remember { mutableStateOf<RegisteredDevice?>(null) }

            LaunchedEffect(savedCredentials) {
                savedCredentials?.let {
                    operatorId.value = it.username
                    operatorPassword.value = it.password
                }
            }

            val diagnostics = SystemDiagnosticsEvaluator.evaluate(
                deviceId = appSettings.deviceId,
                route = endpoint.path.name,
                localDevices = foundDevices.size,
                certificateSha256 = appSettings.localCertificateSha256,
                snapshot = snapshot,
                eventCount = events.size,
            )

            MaterialTheme {
                when {
                    savedCredentials == null -> FirstRunLoginScreen { username, password ->
                        credentials.save(username, password)
                        operatorId.value = username
                        operatorPassword.value = password
                        enrollmentStatus.value = ""
                        rootPage = RootPage.DEVICES
                    }

                    rootPage == RootPage.ADD_DEVICE -> AddDeviceScreen(
                        discovered = foundDevices,
                        busy = addBusy,
                        message = addMessage,
                        onRescan = {
                            enrollmentBusy.value = true
                            enrollmentStatus.value = "Пошук пристроїв…"
                            lifecycleScope.launch {
                                runCatching { discovery.rescan() }
                                enrollmentBusy.value = false
                                enrollmentStatus.value = if (discovery.devices.value.isEmpty()) "HomeGuard у мережі не знайдено" else "Знайдено: ${discovery.devices.value.size}"
                            }
                        },
                        onAddDiscovered = { device, name ->
                            beginEnrollment { done -> registryCoordinator.addIfAuthorized(device, name, onResult = done) }
                        },
                        onAddById = { deviceId, name ->
                            beginEnrollment { done -> registryCoordinator.addById(deviceId, name, discovery.devices.value, done) }
                        },
                        onAddByIp = { ip, name ->
                            beginEnrollment { done -> registryCoordinator.addByIp(ip, name, discovery.devices.value, done) }
                        },
                        onBack = { rootPage = RootPage.DEVICES },
                    )

                    rootPage == RootPage.MONITOR && selectedDevice != null -> DashboardScreen(
                        versionName = BuildConfig.VERSION_NAME,
                        localDevices = foundDevices.size,
                        route = endpoint.path.name,
                        deviceId = selectedDevice!!.displayName,
                        snapshot = snapshot,
                        events = events,
                        diagnostics = diagnostics,
                        backupStatus = maintenanceMessage,
                        commandStatus = commandMessage,
                        operatorId = currentOperator,
                        operatorPin = currentPassword,
                        accessSession = currentAccessSession,
                        criticalNotificationsEnabled = appSettings.criticalNotificationsEnabled,
                        statusNotificationsEnabled = appSettings.statusNotificationsEnabled,
                        zoneNotificationsEnabled = appSettings.zoneNotificationsEnabled,
                        onOperatorIdChange = { },
                        onOperatorPinChange = { },
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

                    else -> RegisteredDevicesScreen(
                        devices = devices,
                        onAddDevice = {
                            enrollmentStatus.value = ""
                            rootPage = RootPage.ADD_DEVICE
                        },
                        onOpenDevice = { device ->
                            selectedDevice = device
                            lifecycleScope.launch {
                                settings.update(
                                    settings.settings.value.copy(
                                        deviceId = device.deviceId,
                                        lastKnownLocalUrl = device.lastKnownUrl,
                                        localCertificateSha256 = device.certificateSha256,
                                    )
                                )
                                accessSession.value = null
                                rootPage = RootPage.MONITOR
                                loginOperator()
                            }
                        },
                    )
                }
            }
        }
    }

    private fun beginEnrollment(action: ((DeviceAddResult) -> Unit) -> Unit) {
        enrollmentBusy.value = true
        enrollmentStatus.value = "Перевірка доступу…"
        action { result ->
            enrollmentBusy.value = false
            enrollmentStatus.value = when (result) {
                is DeviceAddResult.Added -> "Додано: ${result.device.displayName}"
                DeviceAddResult.NotFound -> "Помилка: пристрій не знайдено"
                DeviceAddResult.CredentialsRejected -> "Помилка: користувач або пароль не збігаються"
                DeviceAddResult.AccessRevoked -> "Помилка: доступ цього користувача відкликано"
                is DeviceAddResult.Failed -> "Помилка: ${result.message}"
            }
        }
    }

    private fun loginOperator() {
        val actor = operatorId.value.trim()
        val credential = operatorPassword.value
        if (actor.isBlank() || credential.isBlank()) {
            commandStatus.value = "Дані користувача відсутні"
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
        commandStatus.value = "Сеанс завершено"
    }

    private fun executeCommand(type: CommandType) {
        val actor = operatorId.value.trim()
        val credential = operatorPassword.value
        val authenticated = accessSession.value
        if (authenticated == null || authenticated.actor != actor) {
            commandStatus.value = "Спочатку увійдіть"
            return
        }
        if (!authenticated.allows(type)) {
            commandStatus.value = "Недоступно для ролі ${authenticated.role.name.lowercase()}"
            return
        }
        if (credential.isBlank()) {
            accessSession.value = null
            commandStatus.value = "Пароль відсутній"
            return
        }

        lifecycleScope.launch {
            commandStatus.value = "Виконується: ${type.name}…"
            val result = runCatching { commands.execute(type, actor, credential) }
            commandStatus.value = result.fold(
                onSuccess = { reply ->
                    if (reply.accepted || reply.duplicate) "OK: ${reply.code}"
                    else {
                        if (reply.code.contains("unauthorized", ignoreCase = true) || reply.code.contains("credential", ignoreCase = true)) {
                            accessSession.value = null
                        }
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

    override fun onDestroy() {
        accessSession.value = null
        session.stop()
        discovery.stop()
        super.onDestroy()
    }
}
