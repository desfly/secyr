package ua.homeguard.s3.control

import kotlinx.coroutines.flow.StateFlow
import org.json.JSONObject
import ua.homeguard.s3.model.AccessCapabilities
import ua.homeguard.s3.model.AccessLifecycleState
import ua.homeguard.s3.model.AccessRole
import ua.homeguard.s3.model.AccessSession
import ua.homeguard.s3.model.CommandReply
import ua.homeguard.s3.model.CommandType
import ua.homeguard.s3.model.ControlPath
import ua.homeguard.s3.model.DeviceCommand
import ua.homeguard.s3.model.DeviceEndpoint
import ua.homeguard.s3.network.HttpDeviceApi
import ua.homeguard.s3.network.LocalTelemetryTicketBroker
import ua.homeguard.s3.network.ble.BleHomeGuardClient
import ua.homeguard.s3.network.ble.BleRuntimeDiagnostics
import ua.homeguard.s3.network.ble.BleRuntimeRegistry
import ua.homeguard.s3.storage.SettingsStore
import java.util.concurrent.atomic.AtomicLong

class CommandController(
    private val endpoint: StateFlow<DeviceEndpoint>,
    private val settings: SettingsStore,
) {
    private val requestIds = AtomicLong(System.currentTimeMillis())
    private val ble = BleRuntimeRegistry.get(settings.appContext)
    @Volatile private var localHttpSessionToken: String = ""
    @Volatile private var localActor: String = ""

    init {
        LocalTelemetryTicketBroker.install { refreshTelemetryToken() }
    }

    fun bleState(): StateFlow<BleHomeGuardClient.State> = ble.state()

    suspend fun accessState(): AccessLifecycleState {
        clearLocalSession()
        val target = endpoint.value

        // A LAST_KNOWN_LOCAL address is only cached routing metadata; it is not
        // evidence that the phone can still reach that LAN. Probing it first made
        // the access gate block for the full HTTP timeout when Wi-Fi was disabled,
        // so a registered controller never got a chance to enter through BLE.
        // For a stale local route expose the normal login gate immediately and let
        // login() establish the independent BLE session.
        if (target.path == ControlPath.LAST_KNOWN_LOCAL && canAttemptBleLogin()) {
            return AccessLifecycleState.LOGIN_REQUIRED
        }

        if (target.path != ControlPath.OFFLINE &&
            target.path != ControlPath.CLOUD &&
            target.apiBaseUrl.isNotBlank()
        ) {
            val localState = runCatching { createApi(target).accessState() }.getOrNull()
            if (localState != null) return localState
        }

        return if (canAttemptBleLogin()) {
            AccessLifecycleState.LOGIN_REQUIRED
        } else {
            AccessLifecycleState.UNAVAILABLE
        }
    }

    suspend fun bootstrapAdmin(id: String, name: String, pin: String) {
        val target = localTarget()
        clearLocalSession()
        createApi(target).bootstrapAdmin(id, name, pin)
    }

    suspend fun setupWifiScan(): JSONObject {
        val target = localTarget()
        clearLocalSession()
        return createApi(target).setupWifiScan()
    }

    suspend fun setupConfigureWifi(ssid: String, password: String): JSONObject {
        val target = localTarget()
        clearLocalSession()
        return createApi(target).setupConfigureWifi(ssid, password)
    }

    suspend fun login(actor: String, credential: String): AccessSession {
        val normalizedActor = actor.trim()
        require(normalizedActor.isNotBlank()) { "User ID is required" }
        require(credential.length in 4..12 && credential.all(Char::isDigit)) { "PIN must contain 4-12 digits" }

        clearLocalSession()
        ble.disconnect()
        val target = endpoint.value
        var httpFailure: Throwable? = null
        var bleFailure: Throwable? = null

        // LAST_KNOWN_LOCAL is deliberately BLE-first and BLE-only. The cached
        // 192.168.x.x endpoint may belong to Wi-Fi or W5500 and can remain in the
        // resolver after the phone has left that LAN. Waiting 8-12 seconds on that
        // stale HTTP address defeats independent BLE operation and produced the
        // exact hardware symptom seen on the phone. A currently discovered LOCAL
        // endpoint still keeps HTTP-first behaviour below.
        if (target.path == ControlPath.LAST_KNOWN_LOCAL || target.path == ControlPath.OFFLINE) {
            return loginOverBle(normalizedActor, credential)
        }

        if (target.path != ControlPath.OFFLINE && target.apiBaseUrl.isNotBlank()) {
            val api = createApi(target)
            val httpSession = runCatching { api.login(normalizedActor, credential) }
                .onFailure { httpFailure = it }
                .getOrNull()
            if (httpSession != null) {
                if (target.path != ControlPath.CLOUD) {
                    localHttpSessionToken = httpSession.sessionToken
                    localActor = httpSession.actor
                    val telemetryToken = api.telemetrySession(httpSession.actor)
                    settings.update(settings.settings.value.copy(telemetryToken = telemetryToken))

                    val deviceId = settings.settings.value.deviceId
                    if (deviceId.isNotBlank()) {
                        runCatching {
                            ble.connectAndAuthenticate(
                                deviceId = deviceId,
                                actor = httpSession.actor,
                                pin = credential,
                                connectTimeoutMs = 4_000L,
                                authTimeoutMs = 4_000L,
                            )
                        }
                    }
                }
                return httpSession
            }
        }

        val deviceId = settings.settings.value.deviceId
        require(deviceId.isNotBlank()) {
            httpFailure?.message ?: "controller offline and BLE device id unavailable"
        }

        val bleReply = runCatching {
            ble.connectAndAuthenticate(
                deviceId = deviceId,
                actor = normalizedActor,
                pin = credential,
                connectTimeoutMs = 12_000L,
                authTimeoutMs = 8_000L,
            )
        }.onFailure { bleFailure = it }
            .getOrNull()

        if (bleReply != null) return parseBleAccessSession(bleReply, normalizedActor)

        val httpReason = httpFailure?.message?.takeIf { it.isNotBlank() }
        val bleReason = bleFailure?.message?.takeIf { it.isNotBlank() }
        throw IllegalStateException(
            listOfNotNull(httpReason, bleReason).joinToString("; ").ifBlank { "HTTP and BLE login unavailable" },
            bleFailure ?: httpFailure,
        )
    }

    suspend fun refreshTelemetryToken(): String {
        val target = localTarget()
        require(localHttpSessionToken.isNotBlank() && localActor.isNotBlank()) {
            "authenticated local HTTP session unavailable"
        }
        return issueFreshTelemetryTicket(target)
    }

    fun logout() {
        clearLocalSession()
        ble.disconnect()
    }

    suspend fun execute(type: CommandType, actor: String = "", credential: String = ""): CommandReply {
        val httpResult = runCatching { executeHttp(type, actor, credential) }
        val httpReply = httpResult.getOrNull()
        if (httpReply != null && httpReply.code != "offline" && httpReply.code != "authorization_required") return httpReply

        if (supportsBle(type) && ble.isReady()) {
            val bleReply = runCatching { ble.execute(type) }.getOrNull()
            if (bleReply != null) return mapBleReply(bleReply)
        }

        return httpReply ?: CommandReply(accepted = false, code = "offline")
    }

    suspend fun panicOverBle(): CommandReply {
        if (!ble.isReady()) return CommandReply(accepted = false, code = "ble_not_ready")
        return runCatching { mapBleReply(ble.panic()) }
            .getOrElse { CommandReply(accepted = false, code = it.message ?: "ble_error") }
    }

    suspend fun controlOutputOverBle(
        outputId: Int,
        active: Boolean,
        alarmActive: Boolean = false,
    ): CommandReply {
        if (!ble.isReady()) return CommandReply(accepted = false, code = "ble_not_ready")
        return runCatching { mapBleReply(ble.controlOutput(outputId, active, alarmActive)) }
            .getOrElse { CommandReply(accepted = false, code = it.message ?: "ble_error") }
    }

    suspend fun setLightOverBle(active: Boolean): CommandReply {
        if (!ble.isReady()) return CommandReply(accepted = false, code = "ble_not_ready")
        return runCatching { mapBleReply(ble.setLight(active)) }
            .getOrElse { CommandReply(accepted = false, code = it.message ?: "ble_error") }
    }

    suspend fun setValve1OverBle(active: Boolean): CommandReply {
        if (!ble.isReady()) return CommandReply(accepted = false, code = "ble_not_ready")
        return runCatching { mapBleReply(ble.setValve1(active)) }
            .getOrElse { CommandReply(accepted = false, code = it.message ?: "ble_error") }
    }

    suspend fun setValve2OverBle(active: Boolean): CommandReply {
        if (!ble.isReady()) return CommandReply(accepted = false, code = "ble_not_ready")
        return runCatching { mapBleReply(ble.setValve2(active)) }
            .getOrElse { CommandReply(accepted = false, code = it.message ?: "ble_error") }
    }

    suspend fun pulseLockOverBle(): CommandReply {
        if (!ble.isReady()) return CommandReply(accepted = false, code = "ble_not_ready")
        return runCatching { mapBleReply(ble.pulseLock()) }
            .getOrElse { CommandReply(accepted = false, code = it.message ?: "ble_error") }
    }

    private suspend fun loginOverBle(actor: String, credential: String): AccessSession {
        val deviceId = settings.settings.value.deviceId.trim()
        require(deviceId.isNotBlank() && !deviceId.startsWith("manual-", ignoreCase = true)) {
            "BLE device id unavailable"
        }
        val reply = try {
            ble.connectAndAuthenticate(
                deviceId = deviceId,
                actor = actor,
                pin = credential,
                connectTimeoutMs = 12_000L,
                authTimeoutMs = 8_000L,
            )
        } catch (failure: Throwable) {
            val diagnostic = BleRuntimeDiagnostics.current()
            val path = diagnostic.history.joinToString("→") { transition -> transition.stage }
            val detail = diagnostic.detail.takeIf { it.isNotBlank() }
            val original = failure.message?.takeIf { it.isNotBlank() }
            val message = buildString {
                append("BLE ").append(diagnostic.stage)
                detail?.let { append(": ").append(it) }
                original?.let { append("; ").append(it) }
                if (path.isNotBlank()) append("; path=").append(path)
            }
            throw IllegalStateException(message, failure)
        }
        return parseBleAccessSession(reply, actor)
    }

    private suspend fun executeHttp(type: CommandType, actor: String, credential: String): CommandReply {
        val target = endpoint.value
        val appSettings = settings.settings.value
        if (target.path == ControlPath.OFFLINE || target.apiBaseUrl.isBlank()) return CommandReply(accepted = false, code = "offline")
        if (target.path == ControlPath.CLOUD && appSettings.apiToken.isBlank()) return CommandReply(accepted = false, code = "offline")
        if (target.path != ControlPath.CLOUD && (actor.isBlank() || localHttpSessionToken.isBlank())) {
            return CommandReply(accepted = false, code = "authorization_required")
        }

        val api = createApi(target)
        val challenge = if (target.path == ControlPath.CLOUD && requiresChallenge(type)) api.challenge(type) else null
        val command = DeviceCommand(
            requestId = requestIds.incrementAndGet(),
            issuedAtMs = System.currentTimeMillis(),
            type = type,
            challenge = challenge,
            actor = actor.trim(),
            credential = if (target.path == ControlPath.CLOUD) credential else "",
        )
        return api.command(command)
    }

    private fun parseBleAccessSession(reply: JSONObject, fallbackActor: String): AccessSession {
        val role = when (reply.optString("role", "user").lowercase()) {
            "admin" -> AccessRole.ADMIN
            "guest" -> AccessRole.GUEST
            else -> AccessRole.USER
        }
        val raw = reply.optJSONObject("capabilities")
        val capabilities = if (raw != null) {
            AccessCapabilities(
                monitor = raw.optBoolean("monitor", true),
                armHome = raw.optBoolean("armHome", false),
                armAway = raw.optBoolean("armAway", false),
                disarm = raw.optBoolean("disarm", false),
                panic = raw.optBoolean("panic", false),
                valves = raw.optBoolean("valves", false),
                networkConfigure = raw.optBoolean("networkConfigure", false),
                accessManage = raw.optBoolean("accessManage", false),
                serviceInvalidate = raw.optBoolean("serviceInvalidate", false),
            )
        } else {
            AccessCapabilities(
                monitor = true,
                armHome = role != AccessRole.GUEST,
                armAway = role != AccessRole.GUEST,
                disarm = role != AccessRole.GUEST,
                panic = role != AccessRole.GUEST,
                valves = role != AccessRole.GUEST,
                networkConfigure = role == AccessRole.ADMIN,
                accessManage = role == AccessRole.ADMIN,
                serviceInvalidate = role == AccessRole.ADMIN,
            )
        }
        return AccessSession(
            actor = reply.optString("actor", fallbackActor).ifBlank { fallbackActor },
            name = reply.optString("name", fallbackActor).ifBlank { fallbackActor },
            role = role,
            capabilities = capabilities,
            sessionToken = "",
        )
    }

    private fun mapBleReply(bleReply: JSONObject): CommandReply = CommandReply(
        accepted = bleReply.optBoolean("ok", false),
        duplicate = bleReply.optBoolean("duplicate", false),
        code = bleReply.optString("code").ifBlank {
            bleReply.optString("reason").ifBlank {
                bleReply.optString("status").ifBlank {
                    if (bleReply.optBoolean("ok", false)) "ok_ble" else "rejected_ble"
                }
            }
        },
    )

    private suspend fun issueFreshTelemetryTicket(target: DeviceEndpoint): String {
        val token = createApi(target).telemetrySession(localActor)
        settings.update(settings.settings.value.copy(telemetryToken = token))
        return token
    }

    private fun clearLocalSession() {
        localHttpSessionToken = ""
        localActor = ""
    }

    private fun canAttemptBleLogin(): Boolean {
        val deviceId = settings.settings.value.deviceId.trim()
        return deviceId.isNotBlank() && !deviceId.startsWith("manual-", ignoreCase = true)
    }

    private fun localTarget(): DeviceEndpoint {
        val target = endpoint.value
        require(target.path != ControlPath.OFFLINE && target.path != ControlPath.CLOUD && target.apiBaseUrl.isNotBlank()) {
            "local controller unavailable"
        }
        return target
    }

    private fun createApi(target: DeviceEndpoint): HttpDeviceApi {
        val localRuntime = target.path != ControlPath.CLOUD
        val pin = if (target.path == ControlPath.CLOUD) "" else target.certificateSha256
        return HttpDeviceApi(
            baseUrl = target.apiBaseUrl,
            tokenProvider = { if (localRuntime) localHttpSessionToken else settings.settings.value.apiToken },
            certificatePin = pin,
            runtimeV1 = localRuntime,
        )
    }

    private fun supportsBle(type: CommandType): Boolean = when (type) {
        CommandType.ARM_HOME,
        CommandType.ARM_AWAY,
        CommandType.DISARM,
        CommandType.OPEN_VALVES,
        CommandType.CLOSE_VALVES,
        -> true
        else -> false
    }

    private fun requiresChallenge(type: CommandType): Boolean = when (type) {
        CommandType.DISARM,
        CommandType.RESET_ALARM,
        CommandType.OPEN_VALVES,
        CommandType.ENTER_MAINTENANCE,
        -> true
        else -> false
    }
}
