package androidx.compose.foundation.layout

import androidx.compose.ui.Modifier

/**
 * Compatibility shims for the Compose version used by this project.
 * They keep the project source independent from internal Compose symbols.
 */
fun Modifier.matchParentSize(): Modifier = this.fillMaxSize()

fun Modifier.weight(weight: Float): Modifier = this.fillMaxWidth(weight.coerceIn(0f, 1f))
