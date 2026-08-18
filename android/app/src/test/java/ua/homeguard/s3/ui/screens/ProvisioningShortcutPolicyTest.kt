package ua.homeguard.s3.ui.screens

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ProvisioningShortcutPolicyTest {
    @Test
    fun shortcutRequiresOwnerFriendlyName() {
        assertFalse(canUseProvisioningShortcut("", busy = false))
        assertFalse(canUseProvisioningShortcut("   ", busy = false))
        assertTrue(canUseProvisioningShortcut("Квартира", busy = false))
    }

    @Test
    fun shortcutIsDisabledWhileBusyOrForInvalidManualAddress() {
        assertFalse(canUseProvisioningShortcut("Квартира", busy = true))
        assertFalse(canUseProvisioningShortcut("Квартира", busy = false, addressValid = false))
        assertTrue(canUseProvisioningShortcut("Квартира", busy = false, addressValid = true))
    }
}
