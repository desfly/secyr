package ua.homeguard.s3.model

enum class AccessRole { ADMIN, USER, GUEST }

data class AccessCapabilities(
    val monitor: Boolean = true,
    val armHome: Boolean = false,
    val armAway: Boolean = false,
    val disarm: Boolean = false,
    val panic: Boolean = false,
    val valves: Boolean = false,
    val networkConfigure: Boolean = false,
    val accessManage: Boolean = false,
    val serviceInvalidate: Boolean = false,
) {
    fun allowsOperatorCommand(command: CommandType): Boolean = when (command) {
        CommandType.ARM_HOME -> armHome
        CommandType.ARM_AWAY -> armAway
        CommandType.DISARM -> disarm
        CommandType.OPEN_VALVES, CommandType.CLOSE_VALVES -> valves
        CommandType.SILENCE, CommandType.RESET_ALARM,
        CommandType.ENTER_MAINTENANCE, CommandType.EXIT_MAINTENANCE -> false
    }
}

data class AccessSession(
    val actor: String,
    val name: String,
    val role: AccessRole,
    val capabilities: AccessCapabilities,
) {
    fun allows(command: CommandType): Boolean =
        role == AccessRole.ADMIN || capabilities.allowsOperatorCommand(command)
}
