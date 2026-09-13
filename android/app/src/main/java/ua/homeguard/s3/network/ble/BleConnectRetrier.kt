package ua.homeguard.s3.network.ble

import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.TimeoutCancellationException
import kotlinx.coroutines.delay

/**
 * Runs bounded BLE connection attempts while enforcing the critical ordering:
 * failed GATT teardown must complete before backoff and the next scan.
 */
internal class BleConnectRetrier(
    private val maxAttempts: Int = 2,
    private val backoffMs: Long = 750L,
    private val sleeper: suspend (Long) -> Unit = { delay(it) },
) {
    init {
        require(maxAttempts > 0) { "maxAttempts must be positive" }
        require(backoffMs >= 0L) { "backoffMs must not be negative" }
    }

    suspend fun <T> run(
        attempt: suspend (attemptNumber: Int) -> T,
        cleanupAfterFailure: () -> Unit,
        onRetry: (nextAttempt: Int, failure: Throwable) -> Unit = { _, _ -> },
    ): T {
        for (attemptNumber in 1..maxAttempts) {
            try {
                return attempt(attemptNumber)
            } catch (timeout: TimeoutCancellationException) {
                cleanupAfterFailure()
                if (attemptNumber == maxAttempts) throw timeout
                onRetry(attemptNumber + 1, timeout)
            } catch (cancelled: CancellationException) {
                cleanupAfterFailure()
                throw cancelled
            } catch (failure: Throwable) {
                cleanupAfterFailure()
                if (attemptNumber == maxAttempts) throw failure
                onRetry(attemptNumber + 1, failure)
            }
            sleeper(backoffMs)
        }
        error("unreachable")
    }
}
