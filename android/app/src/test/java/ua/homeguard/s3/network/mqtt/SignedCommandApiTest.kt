package ua.homeguard.s3.network.mqtt

import kotlinx.coroutines.runBlocking
import okhttp3.OkHttpClient
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import okhttp3.tls.HandshakeCertificates
import okhttp3.tls.HeldCertificate
import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Test

class SignedCommandApiTest {
    private fun packet(device: String = "HG-TEST", command: String = "security.arm_away"): String {
        val now = System.currentTimeMillis()
        return JSONObject().put("version", 1).put("deviceId", device).put("requestId", "request-1")
            .put("actor", "session").put("command", command).put("keyEpoch", 1).put("counter", 1)
            .put("issuedAtMs", now).put("expiresAtMs", now + 60_000).put("challenge", "")
            .put("signature", "abcd").toString()
    }

    private fun withServer(block: (MockWebServer, SignedCommandApi) -> Unit) {
        val certificate = HeldCertificate.Builder().addSubjectAlternativeName("localhost").build()
        val serverTls = HandshakeCertificates.Builder().heldCertificate(certificate).build()
        val clientTls = HandshakeCertificates.Builder().addTrustedCertificate(certificate.certificate).build()
        val server = MockWebServer()
        server.useHttps(serverTls.sslSocketFactory(), false)
        server.start()
        val client = OkHttpClient.Builder().sslSocketFactory(clientTls.sslSocketFactory(), clientTls.trustManager).build()
        try { block(server, SignedCommandApi(server.url("/").toString(), { "account-token" }, client)) }
        finally { server.shutdown() }
    }

    @Test fun requestsSignedPacketOverHttpsWithoutSendingActorOrPin() = withServer { server, api ->
        server.enqueue(MockResponse().setBody(packet()))
        val envelope = runBlocking { api.request("HG-TEST", "security.arm_away") }
        assertEquals("HG-TEST", envelope.deviceId)
        val sent = server.takeRequest()
        assertEquals("/v1/devices/HG-TEST/signed-command", sent.path)
        assertEquals("Bearer account-token", sent.getHeader("Authorization"))
        val body = JSONObject(sent.body.readUtf8())
        assertEquals(setOf("command", "challenge"), body.keys().asSequence().toSet())
        assertEquals("security.arm_away", body.getString("command"))
    }

    @Test fun rejectsOtherDeviceOtherCommandAndOversizeResponse() = withServer { server, api ->
        for (body in listOf(packet("HG-OTHER"), packet(command = "security.panic"), "x".repeat(16_385))) {
            server.enqueue(MockResponse().setBody(body))
            try { runBlocking { api.request("HG-TEST", "security.arm_away") }; fail("Invalid packet accepted") }
            catch (_: Exception) { }
        }
        assertEquals(3, server.requestCount)
    }

    @Test fun redirectsAndErrorsAreNotFollowedOrRetried() = withServer { server, api ->
        server.enqueue(MockResponse().setResponseCode(307).addHeader("Location", server.url("/other")))
        try { runBlocking { api.request("HG-TEST", "security.arm_away") }; fail("Redirect accepted") }
        catch (_: java.io.IOException) { }
        assertEquals(1, server.requestCount)
        server.enqueue(MockResponse().setResponseCode(403).setBody("secret account details"))
        try { runBlocking { api.request("HG-TEST", "security.arm_away") }; fail("Error accepted") }
        catch (error: java.io.IOException) { assertFalse(error.message.orEmpty().contains("secret")) }
        assertEquals(2, server.requestCount)
    }

    @Test fun rejectsHttpAndCredentialsInServiceUrl() {
        for (url in listOf("http://localhost/", "https://user:password@localhost/", "https://localhost/?token=secret")) {
            try { SignedCommandApi(url, { "token" }); fail("Invalid URL accepted") }
            catch (_: IllegalArgumentException) { }
        }
    }
}
