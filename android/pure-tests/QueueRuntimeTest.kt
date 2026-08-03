import kotlinx.coroutines.runBlocking
import ua.homeguard.s3.model.CommandType
import ua.homeguard.s3.model.DeviceCommand
import ua.homeguard.s3.queue.OfflineCommandQueue

private var queueChecks = 0
private fun queueCheck(value: Boolean) {
    queueChecks += 1
    if (!value) error("queue check $queueChecks failed")
}

fun main() = runBlocking {
    val queue = OfflineCommandQueue(capacity = 2)
    val first = DeviceCommand(1, 100, CommandType.ARM_HOME)
    val second = DeviceCommand(2, 200, CommandType.DISARM)
    val third = DeviceCommand(3, 300, CommandType.SILENCE)

    queue.enqueue(first)
    queue.enqueue(second)
    queueCheck(queue.size() == 2)
    queue.enqueue(third)
    queueCheck(queue.size() == 2)
    queueCheck(queue.peek(0)?.command?.requestId == 2L)

    queue.enqueue(second.copy(issuedAtMs = 250))
    queueCheck(queue.size() == 2)
    queueCheck(queue.peek(0)?.command?.requestId == 3L)

    queue.markFailure(3, 1_000, "network")
    queueCheck(queue.peek(1_000)?.command?.issuedAtMs == 250L)
    queue.markSuccess(2)
    queueCheck(queue.size() == 1)

    println("HomeGuard Android queue: $queueChecks tests PASS")
}
