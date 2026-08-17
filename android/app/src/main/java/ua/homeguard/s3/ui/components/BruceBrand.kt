package ua.homeguard.s3.ui.components

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier

/**
 * Text-only HomeGuard header.
 *
 * Bruce is intentionally NOT rendered here anymore: Bruce is the full-screen
 * background on application screens, so showing the launcher artwork again in
 * the header created the duplicated "second Bruce" above the content.
 */
@Composable
fun BruceBrand(
    modifier: Modifier = Modifier,
    showTitle: Boolean = false,
) {
    if (!showTitle) return

    Column(
        modifier = modifier.fillMaxWidth(),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Text("MyFist · HomeGuard-S3", style = MaterialTheme.typography.titleMedium)
    }
}
