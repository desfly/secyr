package ua.homeguard.s3.control

import kotlinx.coroutines.flow.StateFlow
import org.json.JSONObject
import ua.homeguard.s3.model.AccessLifecycleState
import ua.homeguard.s3.model.AccessSession
import ua.homeguard.s3.model.CommandReply
import ua.homeguard.s3.model.CommandType
import ua.homeguard.s3.model.ControlPath
import ua.homeguard.s3.model.DeviceCommand
import ua.homeguard.s3.model.DeviceEndpoint
import ua.homeguard.s3.network.HttpDeviceApi
import ua.homeguard.s3.network.LocalTelemetryTicketBroker
import ua.homeguard.s3.network.ble.BleHomeGuardClient
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
        val target = localTarget()
        clearLocalSession()
        return createApi(target).accessState()
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
        val target = endpoint.value
        require(target.path != ControlPath.OFFLINE && target.apiBaseUrl.isNotBlank()) { "controller offline" }

        clearLocalSession()
        ble.disconnect()
        val api = createApi(target)
        val session = api.login(actor, credential)
        if (target.path != ControlPath.CLOUD) {
            localHttpSessionToken = session.sessionToken
            localActor = session.actor
            val telemetryToken = api.telemetrySession(session.actor)
            settings.update(settings.settings.value.copy(telemetryToken = telemetryToken))

            val deviceId = settings.settings.value.deviceId
            if (deviceId.isNotBlank()) {
                runCatching {
                    ble.connectAndAuthenticate(
                        deviceId = deviceId,
                        actor = session.actor,
                        pin = credential,
                        connectTimeoutMs = 4_000L,
                        authTimeoutMs = 4_000L,
                    )
                }
            }
        }
        return session
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
        if (httpReply != null && httpReply.code != "offline") return httpReply

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
