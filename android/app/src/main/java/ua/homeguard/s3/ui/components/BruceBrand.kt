package ua.homeguard.s3.ui.components

import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.size
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.dp
import ua.homeguard.s3.R

@Composable
fun BruceBrand(
    modifier: Modifier = Modifier,
    showTitle: Boolean = false,
) {
    Column(
        modifier = modifier.fillMaxWidth(),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        Image(
            painter = painterResource(R.drawable.bruce_launcher),
            contentDescription = "Bruce",
            modifier = Modifier.size(88.dp),
        )
        if (showTitle) {
            Text("MyFist · HomeGuard-S3", style = MaterialTheme.typography.titleMedium)
        }
    }
}
