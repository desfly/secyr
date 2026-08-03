package ua.homeguard.s3.network

import okhttp3.OkHttpClient
import java.security.MessageDigest
import java.security.SecureRandom
import java.security.cert.CertificateException
import java.security.cert.X509Certificate
import java.time.Duration
import javax.net.ssl.SSLContext
import javax.net.ssl.TrustManager
import javax.net.ssl.X509TrustManager

object PinnedTlsClientFactory {
    fun create(certificateSha256Hex: String, timeoutSeconds: Long = 20): OkHttpClient {
        if (certificateSha256Hex.isBlank()) {
            return OkHttpClient.Builder().callTimeout(Duration.ofSeconds(timeoutSeconds)).build()
        }
        require(certificateSha256Hex.matches(Regex("[0-9a-fA-F]{64}"))) { "invalid_certificate_sha256" }
        val expected = certificateSha256Hex.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
        val trustManager = object : X509TrustManager {
            override fun getAcceptedIssuers(): Array<X509Certificate> = emptyArray()
            override fun checkClientTrusted(chain: Array<X509Certificate>, authType: String) = Unit
            override fun checkServerTrusted(chain: Array<X509Certificate>, authType: String) {
                val certificate = chain.firstOrNull() ?: throw CertificateException("certificate_missing")
                val actual = MessageDigest.getInstance("SHA-256").digest(certificate.encoded)
                if (!MessageDigest.isEqual(expected, actual)) throw CertificateException("certificate_pin_mismatch")
            }
        }
        val context = SSLContext.getInstance("TLS").apply {
            init(null, arrayOf<TrustManager>(trustManager), SecureRandom())
        }
        return OkHttpClient.Builder()
            .sslSocketFactory(context.socketFactory, trustManager)
            .callTimeout(Duration.ofSeconds(timeoutSeconds))
            .build()
    }
}
