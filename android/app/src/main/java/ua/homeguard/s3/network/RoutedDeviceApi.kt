package ua.homeguard.s3.network

import ua.homeguard.s3.model.*

class RoutedDeviceApi(
    private val endpointProvider: () -> DeviceEndpoint,
    private val tokenProvider: () -> String
) : DeviceApi {
    private var cachedKey = ""
    private var cachedApi: HttpDeviceApi? = null

    private fun current(): HttpDeviceApi {
        val endpoint = endpointProvider()
        require(endpoint.path != ControlPath.OFFLINE && endpoint.apiBaseUrl.isNotBlank()) { "device_offline" }
        val key = endpoint.apiBaseUrl + "|" + endpoint.certificateSha256
        if (cachedApi == null || cachedKey != key) {
            cachedKey = key
            cachedApi = HttpDeviceApi(endpoint.apiBaseUrl, tokenProvider, endpoint.certificateSha256)
        }
        return cachedApi!!
    }

    override suspend fun command(command: DeviceCommand) = current().command(command)
    override suspend fun diagnostics() = current().diagnostics()
    override suspend fun snapshot() = current().snapshot()
    override suspend fun challenge(type: CommandType) = current().challenge(type)
}
