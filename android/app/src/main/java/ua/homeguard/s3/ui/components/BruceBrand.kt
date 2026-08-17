package ua.homeguard.s3.ui.components

import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.size
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.dp
import ua.homeguard.s3.R

/** Compact MyFist brand mark for application headers. */
@Composable
fun BruceBrand(
    modifier: Modifier = Modifier,
    showTitle: Boolean = false,
) {
    Row(
        modifier = modifier,
        horizontalArrangement = Arrangement.spacedBy(10.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Image(
            painter = painterResource(R.drawable.bruce_launcher),
            contentDescription = "MyFist",
            contentScale = ContentScale.Crop,
            modifier = Modifier.size(44.dp),
        )
        if (showTitle) {
            Text("MyFist · HomeGuard-S3", style = MaterialTheme.typography.titleMedium)
        }
    }
}
