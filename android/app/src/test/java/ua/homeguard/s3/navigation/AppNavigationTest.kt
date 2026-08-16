package ua.homeguard.s3.navigation

import org.junit.Assert.assertEquals
import org.junit.Test

class AppNavigationTest {
    @Test
    fun freshApplicationStartsOnDeviceList() {
        val navigation = AppNavigation()
        assertEquals(AppScreen.DEVICE_LIST, navigation.screen.value)
    }

    @Test
    fun selectedDeviceOpensDashboardAndBackReturnsToList() {
        val navigation = AppNavigation()
        navigation.onDeviceSelected()
        assertEquals(AppScreen.DASHBOARD, navigation.screen.value)
        navigation.showDeviceList()
        assertEquals(AppScreen.DEVICE_LIST, navigation.screen.value)
    }

    @Test
    fun factoryResetDisconnectAlwaysReturnsToSafeList() {
        val navigation = AppNavigation()
        navigation.onDeviceSelected()
        assertEquals(AppScreen.DASHBOARD, navigation.screen.value)
        navigation.onFactoryResetDisconnect()
        assertEquals(AppScreen.DEVICE_LIST, navigation.screen.value)
    }

    @Test
    fun hiddenAddRouteRequiresExplicitInternalNavigation() {
        val navigation = AppNavigation()
        assertEquals(AppScreen.DEVICE_LIST, navigation.screen.value)
        navigation.showAddDevice()
        assertEquals(AppScreen.ADD_DEVICE, navigation.screen.value)
        navigation.showDeviceList()
        assertEquals(AppScreen.DEVICE_LIST, navigation.screen.value)
    }
}
