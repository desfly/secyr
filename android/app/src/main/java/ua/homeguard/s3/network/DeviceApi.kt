package ua.homeguard.s3.network
import ua.homeguard.s3.model.*
interface DeviceApi { suspend fun command(command:DeviceCommand):CommandReply; suspend fun diagnostics():Diagnostics; suspend fun snapshot():SystemSnapshot; suspend fun challenge(type:CommandType):Long }
