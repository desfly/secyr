package ua.homeguard.s3.queue

import ua.homeguard.s3.model.DeviceCommand

data class QueuedCommand(
    val command: DeviceCommand,
    val attempts: Int = 0,
    val nextAttemptAtMs: Long = 0,
    val lastError: String? = null,
    val enqueuedAtMs: Long = command.issuedAtMs,
    val terminal: Boolean = false,
)
