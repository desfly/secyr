import ua.homeguard.s3.provisioning.HandoffState
import ua.homeguard.s3.provisioning.ProvisioningHandoff
import ua.homeguard.s3.network.LocalApiContract
import ua.homeguard.s3.network.EndpointAvailability
import ua.homeguard.s3.network.selectControlPath
import ua.homeguard.s3.model.ControlPath

private var checks = 0
private fun checkThat(value: Boolean) {
    checks += 1
    if (!value) error("check $checks failed")
}

fun main() {
    val handoff = ProvisioningHandoff("HG-S3-7A31BC", 60_000)
    checkThat(handoff.applyAccepted(1_000).state == HandoffState.WAITING_FOR_REBOOT)
    checkThat(handoff.beginDiscovery(2_000).state == HandoffState.DISCOVERING_LOCAL)
    checkThat(!handoff.observe("HG-S3-OTHER", true, false, "https://192.168.1.20:443", 3_000).accepted)
    checkThat(!handoff.observe("HG-S3-7A31BC", false, false, "http://192.168.1.20:80", 3_100).accepted)
    checkThat(!handoff.observe("HG-S3-7A31BC", true, true, "https://192.168.1.20:443", 3_200).accepted)
    val accepted = handoff.observe("HG-S3-7A31BC", true, false, "https://192.168.1.20:443", 3_300)
    checkThat(accepted.accepted)
    checkThat(accepted.state == HandoffState.COMPLETE)
    checkThat(accepted.localUrl == "https://192.168.1.20:443")
    checkThat(handoff.tick(100_000).state == HandoffState.COMPLETE)

    val timeout = ProvisioningHandoff("HG-S3-7A31BC", 100)
    checkThat(timeout.applyAccepted(0).state == HandoffState.WAITING_FOR_REBOOT)
    checkThat(timeout.beginDiscovery(50).state == HandoffState.DISCOVERING_LOCAL)
    checkThat(timeout.tick(101).state == HandoffState.TIMED_OUT)
    checkThat(!timeout.observe("HG-S3-7A31BC", true, false, "https://192.168.1.20:443", 102).accepted)

    checkThat(LocalApiContract.STATUS_PATH == "/api/status")
    checkThat(LocalApiContract.HEALTH_PATH == "/api/health")
    checkThat(LocalApiContract.CHALLENGE_PATH == "/api/challenge")
    checkThat(LocalApiContract.COMMAND_PATH == "/api/command")
    checkThat(LocalApiContract.TELEMETRY_PATH == "/ws/telemetry")
    checkThat(LocalApiContract.requestId(Long.MAX_VALUE) == "9223372036854775807")

    fun availability(
        secure: Boolean = true,
        api: Int = 1,
        found: Boolean = true,
        last: Boolean = true,
        remote: Boolean = true,
        cloud: Boolean = true,
        id: Boolean = true
    ) = EndpointAvailability(id, found, secure, api, last, remote, cloud)
    checkThat(selectControlPath(availability()) == ControlPath.LOCAL)
    checkThat(selectControlPath(availability(secure = false)) == ControlPath.LAST_KNOWN_LOCAL)
    checkThat(selectControlPath(availability(api = 2)) == ControlPath.LAST_KNOWN_LOCAL)
    checkThat(selectControlPath(availability(found = false, last = false)) == ControlPath.CLOUD)
    checkThat(selectControlPath(availability(found = false, last = false, remote = false)) == ControlPath.OFFLINE)
    checkThat(selectControlPath(availability(found = false, id = false)) == ControlPath.OFFLINE)

    println("HomeGuard Android protocol: $checks tests PASS")
}
