package ua.homeguard.s3.control

import ua.homeguard.s3.model.CommandReply
import ua.homeguard.s3.model.CommandType

object QuickCommandSessionPolicy {
    fun canRecover(type: CommandType, reply: CommandReply): Boolean =
        !reply.accepted && !reply.duplicate && reply.code == "login_required" &&
            type in setOf(CommandType.ARM_HOME, CommandType.ARM_AWAY, CommandType.DISARM)
}
