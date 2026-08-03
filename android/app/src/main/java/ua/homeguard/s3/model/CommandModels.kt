package ua.homeguard.s3.model
enum class CommandType { ARM_HOME, ARM_AWAY, DISARM, SILENCE, OPEN_VALVES, CLOSE_VALVES, RESET_ALARM, ENTER_MAINTENANCE, EXIT_MAINTENANCE }
data class DeviceCommand(val requestId:Long,val issuedAtMs:Long,val type:CommandType,val challenge:Long?=null)
data class CommandReply(val accepted:Boolean,val duplicate:Boolean=false,val code:String="")
