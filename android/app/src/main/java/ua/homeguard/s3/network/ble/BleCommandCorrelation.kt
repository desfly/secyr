package ua.homeguard.s3.network.ble

import org.json.JSONObject
import java.util.UUID

/** One outstanding command per BLE session; old replies cannot finish a newer action. */
class BleCommandCorrelation {
    private var pending: String? = null

    @Synchronized fun begin(): String {
        check(pending == null) { "BLE command already pending" }
        return UUID.randomUUID().toString().also { pending = it }
    }

    @Synchronized fun clear() { pending = null }

    @Synchronized fun accept(reply: JSONObject, error: Boolean = false): Boolean {
        val expected = pending ?: return false
        if (reply.opt("requestId") != expected || (!error && reply.opt("ok") !is Boolean)) return false
        pending = null
        return true
    }
}
