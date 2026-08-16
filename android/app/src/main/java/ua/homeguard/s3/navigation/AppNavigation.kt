package ua.homeguard.s3.navigation

enum class AppScreen {
    DEVICE_LIST,
    ADD_DEVICE,
    PROVISIONING,
    DASHBOARD,
}

class AppNavigation(initial: AppScreen = AppScreen.DEVICE_LIST) {
    var current: AppScreen = initial
        private set

    fun showDeviceList() {
        current = AppScreen.DEVICE_LIST
    }

    fun showAddDevice() {
        current = AppScreen.ADD_DEVICE
    }

    fun showProvisioning() {
        current = AppScreen.PROVISIONING
    }

    fun showDashboard() {
        current = AppScreen.DASHBOARD
    }

    fun onProvisioningFinished() {
        current = AppScreen.ADD_DEVICE
    }

    fun onDeviceSelected() {
        current = AppScreen.DASHBOARD
    }

    fun onDeviceSaved() {
        current = AppScreen.DEVICE_LIST
    }

    fun onFactoryResetDisconnect() {
        current = AppScreen.DEVICE_LIST
    }
}
