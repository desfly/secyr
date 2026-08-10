package ua.homeguard.s3.security

data class CloudAdminUser(
    val id: String,
    val name: String,
    val role: CloudAccessRole,
    val enabled: Boolean,
) {
    companion object {
        fun fromController(id: String, name: String, role: String, enabled: Boolean): CloudAdminUser =
            CloudAdminUser(
                id = id,
                name = name,
                role = when (role.lowercase()) {
                    "admin" -> CloudAccessRole.ADMIN
                    "user" -> CloudAccessRole.USER
                    "guest" -> CloudAccessRole.GUEST
                    else -> CloudAccessRole.LOCKED
                },
                enabled = enabled,
            )
    }
}
