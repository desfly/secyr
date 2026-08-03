package ua.homeguard.s3.queue

import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import ua.homeguard.s3.model.DeviceCommand

class OfflineCommandQueue(
    private val capacity: Int = 64,
    private val retryPolicy: RetryPolicy = RetryPolicy(),
) {
    private val lock = Mutex()
    private val items = mutableListOf<QueuedCommand>()

    init {
        require(capacity > 0)
    }

    suspend fun enqueue(command: DeviceCommand) = lock.withLock {
        items.removeAll { it.command.requestId == command.requestId }

        while (items.size >= capacity) {
            val removable = items.indexOfFirst { !it.terminal }
                .takeIf { it >= 0 } ?: 0
            items.removeAt(removable)
        }

        items.add(QueuedCommand(command = command))
    }

    suspend fun peek(nowMs: Long): QueuedCommand? = lock.withLock {
        items
            .asSequence()
            .filter { !it.terminal && it.nextAttemptAtMs <= nowMs }
            .minByOrNull { it.enqueuedAtMs }
    }

    suspend fun markSuccess(requestId: Long) = lock.withLock {
        items.removeAll { it.command.requestId == requestId }
    }

    suspend fun markFailure(
        requestId: Long,
        nowMs: Long,
        error: String,
        retryAfterMs: Long? = null,
    ) = lock.withLock {
        val index = items.indexOfFirst {
            it.command.requestId == requestId
        }
        if (index < 0) return@withLock

        val old = items[index]
        val decision = retryPolicy.afterFailure(
            attempts = old.attempts,
            nowMs = nowMs,
            retryAfterMs = retryAfterMs,
        )

        items[index] = old.copy(
            attempts = old.attempts + 1,
            nextAttemptAtMs = decision.nextAttemptAtMs,
            lastError = decision.terminalReason ?: error,
            terminal = !decision.retry,
        )
    }

    suspend fun removeTerminal(): Int = lock.withLock {
        val before = items.size
        items.removeAll { it.terminal }
        before - items.size
    }

    suspend fun snapshot(): List<QueuedCommand> = lock.withLock {
        items.toList()
    }

    suspend fun size(): Int = lock.withLock { items.size }
}
