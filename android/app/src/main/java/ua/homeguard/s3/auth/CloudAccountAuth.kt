package ua.homeguard.s3.auth

import com.google.firebase.auth.FirebaseAuth
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

/**
 * Cloud-account credentials are intentionally separate from the controller's
 * local API token. Firebase ID tokens are short-lived and refreshed by the SDK.
 */
object CloudAccountAuth {
    fun signedIn(): Boolean = FirebaseAuth.getInstance().currentUser != null

    suspend fun signIn(email: String, password: String) {
        require(email.isNotBlank() && password.isNotBlank()) { "account_credentials_required" }
        suspendCancellableCoroutine<Unit> { continuation ->
            FirebaseAuth.getInstance().signInWithEmailAndPassword(email.trim(), password)
                .addOnSuccessListener { if (continuation.isActive) continuation.resume(Unit) }
                .addOnFailureListener { if (continuation.isActive) continuation.resumeWithException(it) }
        }
    }

    suspend fun createAccount(email: String, password: String) {
        require(email.isNotBlank() && password.isNotBlank()) { "account_credentials_required" }
        suspendCancellableCoroutine<Unit> { continuation ->
            FirebaseAuth.getInstance().createUserWithEmailAndPassword(email.trim(), password)
                .addOnSuccessListener { if (continuation.isActive) continuation.resume(Unit) }
                .addOnFailureListener { if (continuation.isActive) continuation.resumeWithException(it) }
        }
    }

    suspend fun idToken(forceRefresh: Boolean = false): String {
        val user = FirebaseAuth.getInstance().currentUser
            ?: throw IllegalStateException("cloud_account_not_signed_in")
        return suspendCancellableCoroutine { continuation ->
            user.getIdToken(forceRefresh)
                .addOnSuccessListener {
                    val token = it.token.orEmpty()
                    if (!continuation.isActive) return@addOnSuccessListener
                    if (token.isBlank()) continuation.resumeWithException(IllegalStateException("cloud_account_token_missing"))
                    else continuation.resume(token)
                }
                .addOnFailureListener { if (continuation.isActive) continuation.resumeWithException(it) }
        }
    }

    fun signOut() = FirebaseAuth.getInstance().signOut()
}
