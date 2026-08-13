package ua.homeguard.s3.model

import org.junit.Assert.assertEquals
import org.junit.Test

class AccessLoginFailureTest {
    @Test
    fun deletedUserIsRevoked() {
        assertEquals(AccessLoginFailureReason.USER_NOT_FOUND, accessLoginFailureReason("USER_NOT_FOUND"))
        assertEquals(AccessLoginFailureReason.USER_NOT_FOUND, accessLoginFailureReason("unknown user"))
    }

    @Test
    fun disabledUserIsRevoked() {
        assertEquals(AccessLoginFailureReason.ACCESS_REVOKED, accessLoginFailureReason("ACCESS_REVOKED"))
        assertEquals(AccessLoginFailureReason.ACCESS_REVOKED, accessLoginFailureReason("user disabled"))
    }

    @Test
    fun badPasswordIsNotRevoked() {
        assertEquals(AccessLoginFailureReason.BAD_CREDENTIALS, accessLoginFailureReason("BAD_PASSWORD"))
        assertEquals(AccessLoginFailureReason.BAD_CREDENTIALS, accessLoginFailureReason("invalid credential"))
    }
}
