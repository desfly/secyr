package ua.homeguard.s3.ui

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.launch
import ua.homeguard.s3.model.CommandType
import ua.homeguard.s3.model.Diagnostics
import ua.homeguard.s3.model.SystemSnapshot
import ua.homeguard.s3.repository.HomeGuardRepository

data class UiState(
    val snapshot: SystemSnapshot = SystemSnapshot(),
    val diagnostics: Diagnostics? = null,
    val connected: Boolean = false,
    val message: String? = null
)

class HomeGuardViewModel(private val repo: HomeGuardRepository) : ViewModel() {
    private val _state = MutableStateFlow(UiState())
    val state: StateFlow<UiState> = _state.asStateFlow()

    init {
        viewModelScope.launch {
            repo.telemetry().collect { snapshot ->
                _state.value = _state.value.copy(snapshot = snapshot, connected = true)
            }
        }
    }

    fun command(type: CommandType, dangerous: Boolean = false) = viewModelScope.launch {
        val result = repo.send(type, _state.value.connected, dangerous)
        _state.value = _state.value.copy(message = result.code)
    }

    fun refreshDiagnostics() = viewModelScope.launch {
        val diagnostics = repo.diagnostics()
        _state.value = _state.value.copy(diagnostics = diagnostics)
    }

    fun onReconnected() = viewModelScope.launch { repo.flush() }
}
