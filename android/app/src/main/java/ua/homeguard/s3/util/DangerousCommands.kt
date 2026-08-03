package ua.homeguard.s3.util
import ua.homeguard.s3.model.CommandType
fun CommandType.isDangerous()=this==CommandType.OPEN_VALVES||this==CommandType.RESET_ALARM||this==CommandType.EXIT_MAINTENANCE
