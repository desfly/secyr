package ua.homeguard.s3.model

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class AccessSessionControllerBindingTest {
    private val session = AccessSession(
        actor = "admin",
        name = "Admin",
        role = AccessRole.ADMIN,
        capabilities = AccessCapabilities(),
        controllerId = "HG-AbC123",
    )

    @Test
    fun sameControllerIdIsCaseInsensitive() {
        assertTrue(session.belongsTo("hg-abc123"))
    }

    @Test
    fun differentOrBlankControllerIsRejected() {
        assertFalse(session.belongsTo("HG-OTHER"))
        assertFalse(session.belongsTo(""))
        assertFalse(session.copy(controllerId = "").belongsTo("HG-AbC123"))
    }
}
