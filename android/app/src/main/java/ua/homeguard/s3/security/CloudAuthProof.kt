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

    fun canonicalTarget(targetId: String): String = canonicalField(targetId)

    fun canonicalUpsert(
        targetId: String,
        name: String,
        role: String,
        enabled: Boolean,
        encryptedPinHex: String,
    ): String = buildString {
        append(canonicalField(targetId))
        append(canonicalField(name))
        append(canonicalField(role))
        append(canonicalField(enabled.toString()))
        append(canonicalField(encryptedPinHex))
    }

    fun adminPayloadProof(
        requestId: String,
        command: String,
        pinDigestHex: String,
        canonicalPayload: String,
    ): String {
        require(requestId.isNotBlank())
        require(command.isNotBlank())
        require(pinDigestHex.length == 64)
        return hex(sha256("HomeGuard-S3|ADMIN-PAYLOAD|${pinDigestHex.lowercase()}|$requestId|$command|$canonicalPayload"))
    }

    fun encryptAdminPin(
        pin: String,
        requestId: String,
        command: String,
        pinDigestHex: String,
    ): String {
        require(pin.length in 4..12)
        require(pinDigestHex.length == 64)
        val key = sha256("HomeGuard-S3|ADMIN-NEW-PIN|${pinDigestHex.lowercase()}|$requestId|$command")
        val bytes = pin.toByteArray(Charsets.UTF_8)
        require(bytes.size in 4..12 && bytes.size <= key.size)
        return buildString(bytes.size * 2) {
            bytes.forEachIndexed { index, value ->
                append("%02x".format((value.toInt() and 0xff) xor (key[index].toInt() and 0xff)))
            }
        }
    }

    private fun canonicalField(value: String): String =
        "${value.toByteArray(Charsets.UTF_8).size}:$value|"

    private fun sha256(text: String): ByteArray = MessageDigest.getInstance("SHA-256")
        .digest(text.toByteArray(Charsets.UTF_8))

    private fun hex(bytes: ByteArray): String = buildString(bytes.size * 2) {
        for (value in bytes) append("%02x".format(value.toInt() and 0xff))
    }
}
