package ua.homeguard.s3.data

import java.util.UUID
import ua.homeguard.s3.api.HomeGuardApi
import ua.homeguard.s3.api.model.CommandRequestDto
import ua.homeguard.s3.api.model.CommandResponseDto
import ua.homeguard.s3.api.model.DeviceStateDto

class HomeGuardRepository(
    private val api: HomeGuardApi,
    private val actor: () -> String,
) {
    suspend fun refresh(): DeviceStateDto = api.state()

    suspend fun armHome(): CommandResponseDto =
        execute("security.arm_home")

    suspend fun armAway(): CommandResponseDto =
        execute("security.arm_away")

    suspend fun disarm(): CommandResponseDto =
        execute("security.disarm")

    suspend fun setCorridorLight(enabled: Boolean): CommandResponseDto =
        execute(
            command = "light.set",
            value = if (enabled) "on" else "off",
        )

    suspend fun closeValve(id: String): CommandResponseDto =
        execute("valve.close", target = id)

    suspend fun openValve(id: String): CommandResponseDto =
        execute("valve.open", target = id)

    suspend fun clearValveLatch(id: String): CommandResponseDto =
        execute("valve.clear_latch", target = id)

    private suspend fun execute(
        command: String,
        target: String = "",
        value: String = "",
    ): CommandResponseDto =
        api.command(
            CommandRequestDto(
                requestId = UUID.randomUUID().toString(),
                actor = actor(),
                command = command,
                target = target,
                value = value,
            )
        )
}
