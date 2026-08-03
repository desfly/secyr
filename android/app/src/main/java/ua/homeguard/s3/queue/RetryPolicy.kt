package ua.homeguard.s3.queue

import kotlin.math.min

data class RetryDecision(
    val retry: Boolean,
    val nextAttemptAtMs: Long,
    val terminalReason: String? = null,
)

class RetryPolicy(
    private val baseDelayMs: Long = 1_000,
    private val maximumDelayMs: Long = 60_000,
    private val maximumAttempts: Int = 12,
) {
    init {
        require(baseDelayMs > 0)
        require(maximumDelayMs >= baseDelayMs)
        require(maximumAttempts > 0)
    }

    fun afterFailure(
        attempts: Int,
        nowMs: Long,
        retryAfterMs: Long? = null,
    ): RetryDecision {
        val nextAttempts = attempts + 1
        if (nextAttempts >= maximumAttempts) {
            return RetryDecision(
                retry = false,
                nextAttemptAtMs = nowMs,
                terminalReason = "maximum_attempts",
            )
        }

        val exponent = min(nextAttempts - 1, 20)
        val exponential = baseDelayMs * (1L shl exponent)
        val requested = retryAfterMs?.coerceAtLeast(0) ?: 0
        val delay = maxOf(exponential, requested).coerceAtMost(maximumDelayMs)

        return RetryDecision(
            retry = true,
            nextAttemptAtMs = nowMs + delay,
        )
    }
}
