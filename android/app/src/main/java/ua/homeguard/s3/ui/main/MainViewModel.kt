package ua.homeguard.s3.ui.main

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import ua.homeguard.s3.api.model.CommandResponseDto
import ua.homeguard.s3.data.HomeGuardRepository

class MainViewModel(
    private val scope: CoroutineScope,
    private val repository: HomeGuardRepository,
) {
    private val mutableState = MutableStateFlow(MainUiState())
    val state: StateFlow<MainUiState> = mutableState

    fun refresh() {
        scope.launch {
            mutableState.update { it.copy(loading = true, error = null) }
            runCatching { repository.refresh() }
                .onSuccess { device ->
                    mutableState.value = MainUiState(
                        loading = false,
                        device = device,
                    )
                }
                .onFailure { error ->
                    mutableState.update {
                        it.copy(
                            loading = false,
                            error = error.message ?: "network error",
                        )
                    }
                }
        }
    }

    fun armHome() = command { repository.armHome() }
    fun armAway() = command { repository.armAway() }
    fun disarm() = command { repository.disarm() }
    fun setLight(enabled: Boolean) =
        command { repository.setCorridorLight(enabled) }
    fun closeValve(id: String) =
        command { repository.closeValve(id) }
    fun openValve(id: String) =
        command { repository.openValve(id) }
    fun clearValveLatch(id: String) =
        command { repository.clearValveLatch(id) }

    private fun command(
        action: suspend () -> CommandResponseDto,
    ) {
        scope.launch {
            mutableState.update {
                it.copy(sendingCommand = true, error = null)
            }

            runCatching { action() }
                .onSuccess { response ->
                    mutableState.update {
                        it.copy(
                            sendingCommand = false,
                            message = "${response.code}: ${response.message}",
                        )
                    }
                    refresh()
                }
                .onFailure { error ->
                    mutableState.update {
                        it.copy(
                            sendingCommand = false,
                            error = error.message ?: "command failed",
                        )
                    }
                }
        }
    }
}
