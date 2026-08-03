import ua.homeguard.s3.model.CompileGateStatus
import ua.homeguard.s3.ui.infrastructure.CompileGateFormatter

fun main() {
    val status = CompileGateStatus(
        preflightPassed = true,
        sourceAuditPassed = true,
        dependencyAuditPassed = true,
        mockSyntaxPassed = true,
        realEspIdfBuildPassed = null,
    )

    val text = CompileGateFormatter.summary(status)
    check(text.contains("mock=true"))
    check(text.contains("idf=pending"))
    println("Compile gate formatter tests PASS")
}
