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
    input.hasDeviceId && input.matchingLocalFound && input.matchingLocalSecure && input.matchingLocalApiVersion == 1 -> ControlPath.LOCAL
    input.hasDeviceId && input.hasLastKnownLocal -> ControlPath.LAST_KNOWN_LOCAL
    input.hasDeviceId && input.remoteAccessEnabled && input.hasCloudBaseUrl -> ControlPath.CLOUD
    else -> ControlPath.OFFLINE
}
