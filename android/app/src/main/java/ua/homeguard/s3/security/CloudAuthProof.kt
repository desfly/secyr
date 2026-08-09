package ua.homeguard.s3.security

import java.security.MessageDigest

object CloudAuthProof {
    fun derivePinDigest(userId: String, pin: String, saltHex: String): String {
        require(userId.isNotBlank())
        require(pin.length in 4..12)
        require(saltHex.length == 32)

        var digest = sha256("HomeGuard-S3|PIN|$userId|${saltHex.lowercase()}|$pin")
        repeat(4095) {
            digest = sha256(hex(digest) + userId + saltHex.lowercase())
        }
        return hex(digest)
    }

    fun proof(
        requestId: String,
        command: String,
        nonceHex: String,
        pinDigestHex: String,
    ): String {
        require(requestId.isNotBlank())
        require(command.isNotBlank())
        require(nonceHex.isNotBlank())
        require(pinDigestHex.length == 64)
        val material = "HomeGuard-S3|CLOUD|${pinDigestHex.lowercase()}|${nonceHex.lowercase()}|$requestId|$command"
        return hex(sha256(material))
    }

    private fun sha256(text: String): ByteArray = MessageDigest.getInstance("SHA-256")
        .digest(text.toByteArray(Charsets.UTF_8))

    private fun hex(bytes: ByteArray): String = buildString(bytes.size * 2) {
        for (value in bytes) append("%02x".format(value.toInt() and 0xff))
    }
}
