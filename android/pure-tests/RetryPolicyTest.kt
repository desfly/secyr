import ua.homeguard.s3.queue.RetryPolicy

fun main() {
    val policy = RetryPolicy(
        baseDelayMs = 1_000,
        maximumDelayMs = 60_000,
        maximumAttempts = 4,
    )

    val first = policy.afterFailure(attempts = 0, nowMs = 10_000)
    check(first.retry)
    check(first.nextAttemptAtMs == 11_000)

    val second = policy.afterFailure(attempts = 1, nowMs = 10_000)
    check(second.retry)
    check(second.nextAttemptAtMs == 12_000)

    val serverDelay = policy.afterFailure(
        attempts = 0,
        nowMs = 10_000,
        retryAfterMs = 8_000,
    )
    check(serverDelay.nextAttemptAtMs == 18_000)

    val terminal = policy.afterFailure(attempts = 3, nowMs = 10_000)
    check(!terminal.retry)
    check(terminal.terminalReason == "maximum_attempts")

    println("RetryPolicy tests PASS")
}
