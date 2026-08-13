package ua.homeguard.s3.network

import ua.homeguard.s3.model.AccessSession
import ua.homeguard.s3.model.DiscoveredDevice

class DeviceAccessVerifier {
    suspend fun login(baseUrl: String, username: String, credential: String): AccessSession {
        return HttpDeviceApi(
            baseUrl = baseUrl,
            tokenProvider = { "" },
        ).login(username, credential)
    }

    suspend fun login(device: DiscoveredDevice, username: String, credential: String): AccessSession {
        return login(device.baseUrl, username, credential)
    }
}
