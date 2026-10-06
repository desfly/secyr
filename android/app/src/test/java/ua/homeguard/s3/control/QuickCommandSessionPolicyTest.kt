package ua.homeguard.s3.control

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import ua.homeguard.s3.model.CommandReply
import ua.homeguard.s3.model.CommandType

class QuickCommandSessionPolicyTest {
    @Test fun expiredSessionAllowsIdempotentSecurityRetry() {
        for (type in listOf(CommandType.ARM_HOME, CommandType.ARM_AWAY, CommandType.DISARM)) {
            assertTrue(QuickCommandSessionPolicy.canRecover(type, CommandReply(false, code = "login_required")))
        }
    }
    @Test fun neverRetryPermissionDenialAmbiguousFailureOrAppliedCommand() {
        for (code in listOf("offline", "http_error", "forbidden", "permission_denied")) {
            assertFalse(QuickCommandSessionPolicy.canRecover(CommandType.DISARM, CommandReply(false, code = code)))
        }
        assertFalse(QuickCommandSessionPolicy.canRecover(CommandType.DISARM, CommandReply(true, code = "login_required")))
        assertFalse(QuickCommandSessionPolicy.canRecover(CommandType.DISARM, CommandReply(false, duplicate = true, code = "login_required")))
        assertFalse(QuickCommandSessionPolicy.canRecover(CommandType.OPEN_VALVES, CommandReply(false, code = "login_required")))
        assertFalse(QuickCommandSessionPolicy.canRecover(CommandType.CLOSE_VALVES, CommandReply(false, code = "login_required")))
    }
}
