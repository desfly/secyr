package ua.homeguard.s3.repository

import ua.homeguard.s3.model.AccessLoginFailureReason
import ua.homeguard.s3.model.AccessLoginRejectedException
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.model.RegisteredDeviceAccess
import ua.homeguard.s3.network.HttpDeviceApi
import ua.homeguard.s3.storage.SavedUserCredentials

class DeviceAccessVerifier {
    suspend fun verify(
        device: DiscoveredDevice,
        credentials: SavedUserCredentials,
        certificateSha256: String = "",
    ): RegisteredDeviceAccess {
        val api = HttpDeviceApi(
            baseUrl = device.baseUrl,
            tokenProvider = { "" },
            certificatePin = certificateSha256,
        )
        return try {
            api.login(credentials.username, credentials.password)
            RegisteredDeviceAccess.ONLINE
        } catch (error: AccessLoginRejectedException) {
            when (error.reason) {
                AccessLoginFailureReason.USER_NOT_FOUND,
                AccessLoginFailureReason.ACCESS_REVOKED,
                -> RegisteredDeviceAccess.ACCESS_REVOKED

                AccessLoginFailureReason.BAD_CREDENTIALS,
                AccessLoginFailureReason.UNKNOWN,
                -> RegisteredDeviceAccess.CREDENTIALS_REJECTED
            }
        } catch (_: Exception) {
            RegisteredDeviceAccess.OFFLINE
        }
    }
}
