package ua.homeguard.s3.network

import ua.homeguard.s3.model.ControlPath
import ua.homeguard.s3.model.DeviceCommand
import ua.homeguard.s3.model.DeviceEndpoint

class RoutedDeviceApi(
    private val endpointProvider: () -> DeviceEndpoint,
    private val tokenProvider: () -> String
) : DeviceApi {
    private var cachedKey = ""
    private var cachedApi: HttpDeviceApi? = null

    private fun current(): HttpDeviceApi {
        val endpoint = endpointProvider()
        require(endpoint.path != ControlPath.OFFLINE && endpoint.apiBaseUrl.isNotBlank()) { "device_offline" }
        val runtimeV1 = endpoint.path != ControlPath.CLOUD
        val pin = if (endpoint.path == ControlPath.CLOUD) "" else endpoint.certificateSha256
        val key = endpoint.apiBaseUrl + "|" + pin + "|" + runtimeV1
        if (cachedApi == null || cachedKey != key) {
            cachedKey = key
            cachedApi = HttpDeviceApi(
                baseUrl = endpoint.apiBaseUrl,
                tokenProvider = tokenProvider,
                certificatePin = pin,
                runtimeV1 = runtimeV1,
            )
        }
        return cachedApi!!
    }

    override suspend fun command(command: DeviceCommand) = current().command(command)
    override suspend fun diagnostics() = current().diagnostics()
    override suspend fun snapshot() = current().snapshot()
}
