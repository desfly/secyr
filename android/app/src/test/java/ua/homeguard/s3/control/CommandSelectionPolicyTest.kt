package ua.homeguard.s3.control

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class CommandSelectionPolicyTest {
    @Test
    fun `same controller id is accepted case insensitively`() {
        assertTrue(sameSelectedController("HG-AbC123", "hg-abc123"))
    }

    @Test
    fun `different controller is rejected`() {
        assertFalse(sameSelectedController("HG-ONE", "HG-TWO"))
    }

    @Test
    fun `blank controller id is never treated as a stable selection`() {
        assertFalse(sameSelectedController("", ""))
    }
}
