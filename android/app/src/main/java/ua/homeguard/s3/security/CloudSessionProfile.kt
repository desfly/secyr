package ua.homeguard.s3.security

enum class CloudAccessRole { LOCKED, GUEST, USER, ADMIN }

data class CloudSessionProfile(
    val id: String = "",
    val name: String = "",
    val role: CloudAccessRole = CloudAccessRole.LOCKED,
    val authenticated: Boolean = false,
    val canArm: Boolean = false,
    val canDisarm: Boolean = false,
    val canControlValves: Boolean = false,
    val canManageUsers: Boolean = false,
) {
    val canSubscribeFullState: Boolean
        get() = authenticated && role in setOf(CloudAccessRole.USER, CloudAccessRole.ADMIN)

    val sensorOnly: Boolean
        get() = authenticated && role == CloudAccessRole.GUEST

    companion object {
        fun locked() = CloudSessionProfile()

        fun fromController(
            id: String,
            name: String,
            role: String,
            enabled: Boolean,
            canArm: Boolean,
            canDisarm: Boolean,
            canControlValves: Boolean,
            canManageUsers: Boolean,
        ): CloudSessionProfile {
            if (!enabled) return locked()
            val parsedRole = when (role.lowercase()) {
                "admin" -> CloudAccessRole.ADMIN
                "user" -> CloudAccessRole.USER
                "guest" -> CloudAccessRole.GUEST
                else -> CloudAccessRole.LOCKED
            }
            if (parsedRole == CloudAccessRole.LOCKED) return locked()
            return CloudSessionProfile(
                id = id,
                name = name,
                role = parsedRole,
                authenticated = true,
                canArm = parsedRole != CloudAccessRole.GUEST && canArm,
                canDisarm = parsedRole != CloudAccessRole.GUEST && canDisarm,
                canControlValves = parsedRole != CloudAccessRole.GUEST && canControlValves,
                canManageUsers = parsedRole == CloudAccessRole.ADMIN && canManageUsers,
            )
        }
    }
}
