package androidx.compose.foundation.layout

import androidx.compose.ui.Modifier

/**
 * Compatibility shim for the Compose version used by this project.
 * MyfistBackground only needs the child to occupy the full parent bounds.
 */
fun Modifier.matchParentSize(): Modifier = this.fillMaxSize()
