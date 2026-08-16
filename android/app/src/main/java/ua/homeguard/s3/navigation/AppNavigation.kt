package ua.homeguard.s3.navigation

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

enum class AppScreen {
    DEVICE_LIST,
    ADD_DEVICE,
    PROVISIONING,
    DASHBOARD,
}

class AppNavigation(initial: AppScreen = AppScreen.DEVICE_LIST) {
    private val _screen = MutableStateFlow(initial)
    val screen: StateFlow<AppScreen> = _screen.asStateFlow()

    fun showDeviceList() {
        _screen.value = AppScreen.DEVICE_LIST
    }

    fun showAddDevice() {
        _screen.value = AppScreen.ADD_DEVICE
    }

    fun showProvisioning() {
        _screen.value = AppScreen.PROVISIONING
    }

    fun showDashboard() {
        _screen.value = AppScreen.DASHBOARD
    }

    fun onProvisioningFinished() {
        _screen.value = AppScreen.ADD_DEVICE
    }

    fun onDeviceSelected() {
        _screen.value = AppScreen.DASHBOARD
    }

    fun onDeviceSaved() {
        _screen.value = AppScreen.DEVICE_LIST
    }

    fun onFactoryResetDisconnect() {
        _screen.value = AppScreen.DEVICE_LIST
    }
}