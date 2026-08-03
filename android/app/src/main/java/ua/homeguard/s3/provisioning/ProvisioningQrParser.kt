package ua.homeguard.s3.provisioning

import android.net.Uri
import ua.homeguard.s3.model.ProvisioningQrData

object ProvisioningQrParser {
    private val deviceId = Regex("HG-S3-[0-9A-F]{6}")
    private val fingerprint = Regex("[0-9a-fA-F]{64}")
    private val pairingCode = Regex("[0-9]{8}")

    fun parse(raw: String): ProvisioningQrData {
        val uri = Uri.parse(raw.trim())
        require(uri.scheme == "homeguard" && uri.host == "provision") { "Невідомий QR-код" }
        val version = uri.getQueryParameter("v")?.toIntOrNull() ?: error("Немає версії QR")
        require(version == 1) { "Непідтримувана версія QR" }
        val id = uri.required("id").uppercase()
        val ssid = uri.required("ssid")
        val password = uri.required("pw")
        val url = uri.required("url")
        val fp = uri.required("fp").lowercase()
        val code = uri.required("code")
        require(deviceId.matches(id)) { "Некоректний device_id" }
        require(ssid.length in 1..32) { "Некоректний Setup SSID" }
        require(password.length in 12..63) { "Некоректний пароль Setup AP" }
        require(url.startsWith("https://")) { "Налаштування дозволене лише через HTTPS" }
        require(fingerprint.matches(fp)) { "Некоректний відбиток сертифіката" }
        require(pairingCode.matches(code)) { "Некоректний одноразовий код" }
        return ProvisioningQrData(version, id, ssid, password, url, fp, code)
    }

    private fun Uri.required(name: String): String = getQueryParameter(name)?.takeIf { it.isNotBlank() }
        ?: error("QR не містить $name")
}
