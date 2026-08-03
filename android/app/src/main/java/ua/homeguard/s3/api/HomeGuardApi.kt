package ua.homeguard.s3.api

import ua.homeguard.s3.api.model.BuildInfoDto
import ua.homeguard.s3.api.model.CommandRequestDto
import ua.homeguard.s3.api.model.CommandResponseDto
import ua.homeguard.s3.api.model.DeviceStateDto

interface HomeGuardApi {
    suspend fun state(): DeviceStateDto
    suspend fun build(): BuildInfoDto
    suspend fun command(request: CommandRequestDto): CommandResponseDto
}
