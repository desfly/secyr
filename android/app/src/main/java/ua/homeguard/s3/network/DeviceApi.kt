package ua.homeguard.s3.network

import ua.homeguard.s3.model.CommandReply
import ua.homeguard.s3.model.DeviceCommand
import ua.homeguard.s3.model.Diagnostics
import ua.homeguard.s3.model.SystemSnapshot

interface DeviceApi {
    suspend fun command(command: DeviceCommand): CommandReply
    suspend fun diagnostics(): Diagnostics
    suspend fun snapshot(): SystemSnapshot
}
