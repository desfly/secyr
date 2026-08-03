package ua.homeguard.s3.queue

import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.delay
import ua.homeguard.s3.model.CommandReply
import ua.homeguard.s3.network.DeviceApi

data class FlushReport(
    val delivered: Int,
    val failed: Int,
    val remaining: Int,
)

class CommandQueueFlusher(
    private val api: DeviceApi,
    private val queue: OfflineCommandQueue,
    private val nowMs: () -> Long = System::currentTimeMillis,
) {
    suspend fun flush(
        maximumCommands: Int = 32,
        pauseBetweenCommandsMs: Long = 25,
    ): FlushReport {
        require(maximumCommands > 0)
        require(pauseBetweenCommandsMs >= 0)

        var delivered = 0
        var failed = 0
        var processed = 0

        while (processed < maximumCommands) {
            val now = nowMs()
            val queued = queue.peek(now) ?: break
            processed++

            try {
                val reply: CommandReply = api.command(queued.command)
                if (reply.accepted || reply.duplicate) {
                    queue.markSuccess(queued.command.requestId)
                    delivered++
                } else {
                    queue.markFailure(
                        requestId = queued.command.requestId,
                        nowMs = nowMs(),
                        error = reply.code.ifBlank { "rejected" },
                    )
                    failed++
                }
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (error: Throwable) {
                queue.markFailure(
                    requestId = queued.command.requestId,
                    nowMs = nowMs(),
                    error = error.message ?: "network",
                )
                failed++
                break
            }

            if (pauseBetweenCommandsMs > 0) {
                delay(pauseBetweenCommandsMs)
            }
        }

        return FlushReport(
            delivered = delivered,
            failed = failed,
            remaining = queue.size(),
        )
    }
}
