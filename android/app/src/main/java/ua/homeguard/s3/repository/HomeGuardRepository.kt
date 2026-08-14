package ua.homeguard.s3.repository

import kotlinx.coroutines.flow.Flow
import ua.homeguard.s3.model.CommandReply
import ua.homeguard.s3.model.CommandType
import ua.homeguard.s3.model.Diagnostics
import ua.homeguard.s3.model.DeviceCommand
import ua.homeguard.s3.model.SystemSnapshot
import ua.homeguard.s3.network.DeviceApi
import ua.homeguard.s3.network.RequestIdGenerator
import ua.homeguard.s3.network.TelemetrySocket
import ua.homeguard.s3.queue.OfflineCommandQueue

class HomeGuardRepository(
    private val api: DeviceApi,
    private val socket: TelemetrySocket,
    private val queue: OfflineCommandQueue,
    private val ids: RequestIdGenerator,
) {
    fun telemetry(): Flow<SystemSnapshot> = socket.snapshots()

    suspend fun send(type: CommandType, online: Boolean, dangerous: Boolean = false): CommandReply {
        val command = DeviceCommand(
            requestId = ids.next(),
            issuedAtMs = System.currentTimeMillis(),
            type = type,
            challenge = null,
        )
        if (!online) {
            queue.enqueue(command)
            return CommandReply(false, code = "queued")
        }
        val reply = api.command(command)
        if (!reply.accepted && !reply.duplicate) queue.enqueue(command)
        return reply
    }

    suspend fun flush() {
        while (true) {
            val item = queue.peek(System.currentTimeMillis()) ?: break
            runCatching { api.command(item.command) }
                .onSuccess {
                    if (it.accepted || it.duplicate) {
                        queue.markSuccess(item.command.requestId)
                    } else {
                        queue.markFailure(item.command.requestId, System.currentTimeMillis(), it.code)
                    }
                }
                .onFailure {
                    queue.markFailure(
                        item.command.requestId,
                        System.currentTimeMillis(),
                        it.message ?: "network",
                    )
                }
            if (queue.peek(System.currentTimeMillis())?.command?.requestId == item.command.requestId) break
        }
    }

    suspend fun diagnostics(): Diagnostics = api.diagnostics().copy(queuedCommands = queue.size())
}
