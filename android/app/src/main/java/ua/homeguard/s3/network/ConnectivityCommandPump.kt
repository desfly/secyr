package ua.homeguard.s3.network

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.launch
import ua.homeguard.s3.queue.CommandQueueFlusher

class ConnectivityCommandPump(
    private val scope: CoroutineScope,
    private val online: Flow<Boolean>,
    private val flusher: CommandQueueFlusher,
) {
    private var job: Job? = null

    fun start() {
        if (job != null) return

        job = scope.launch {
            online
                .distinctUntilChanged()
                .collect { connected ->
                    if (connected) {
                        flusher.flush()
                    }
                }
        }
    }

    fun stop() {
        job?.cancel()
        job = null
    }
}
