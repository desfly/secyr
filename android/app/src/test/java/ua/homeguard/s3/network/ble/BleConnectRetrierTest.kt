package ua.homeguard.s3.network.ble

import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test

class BleConnectRetrierTest {
    @Test
    fun closesFailedGattBeforeBackoffAndNextScan() = runBlocking {
        val events = mutableListOf<String>()
        val retrier = BleConnectRetrier(
            maxAttempts = 2,
            backoffMs = 750L,
            sleeper = { events += "backoff:$it" },
        )

        val result = retrier.run(
            attempt = { attempt ->
                events += "scan:$attempt"
                if (attempt == 1) error("security failed")
                "connected"
            },
            cleanupAfterFailure = { events += "close-gatt" },
            onRetry = { nextAttempt, _ -> events += "retry:$nextAttempt" },
        )

        assertEquals("connected", result)
        assertEquals(
            listOf("scan:1", "close-gatt", "retry:2", "backoff:750", "scan:2"),
            events,
        )
    }

    @Test
    fun closesGattAfterFinalFailure() {
        var cleanupCount = 0
        assertThrows(IllegalStateException::class.java) {
            runBlocking {
                BleConnectRetrier(maxAttempts = 2, sleeper = {}).run<Unit>(
                    attempt = { error("failed") },
                    cleanupAfterFailure = { cleanupCount += 1 },
                )
            }
        }
        assertEquals(2, cleanupCount)
    }

    @Test
    fun externalCancellationIsNeverRetried() {
        var attempts = 0
        var cleanupCount = 0
        assertThrows(CancellationException::class.java) {
            runBlocking {
                BleConnectRetrier(maxAttempts = 2, sleeper = {}).run<Unit>(
                    attempt = {
                        attempts += 1
                        throw CancellationException("cancelled")
                    },
                    cleanupAfterFailure = { cleanupCount += 1 },
                )
            }
        }
        assertEquals(1, attempts)
        assertEquals(1, cleanupCount)
    }
}
