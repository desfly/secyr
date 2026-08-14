package ua.homeguard.s3.network

import ua.homeguard.s3.model.ControlPath

data class EndpointAvailability(
    val hasDeviceId: Boolean,
    val matchingLocalFound: Boolean,
    val matchingLocalSecure: Boolean,
    val matchingLocalApiVersion: Int,
    val hasLastKnownLocal: Boolean,
    val remoteAccessEnabled: Boolean,
    val hasCloudBaseUrl: Boolean
)

fun selectControlPath(input: EndpointAvailability): ControlPath = when {
    // HomeGuard emergency AP and current LAN firmware expose the local API over HTTP.
    // Do not reject a discovered controller merely because TLS is disabled locally.
    input.hasDeviceId && input.matchingLocalFound && input.matchingLocalApiVersion == 1 -> ControlPath.LOCAL
    input.hasDeviceId && input.hasLastKnownLocal -> ControlPath.LAST_KNOWN_LOCAL
    input.hasDeviceId && input.remoteAccessEnabled && input.hasCloudBaseUrl -> ControlPath.CLOUD
    else -> ControlPath.OFFLINE
}
