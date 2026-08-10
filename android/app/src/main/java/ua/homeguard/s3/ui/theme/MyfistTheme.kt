package ua.homeguard.s3.ui.theme

import android.graphics.BitmapFactory
import android.util.Base64
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.matchParentSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale

private val MyfistColors = darkColorScheme(
    primary = Color(0xFF27D39B),
    onPrimary = Color(0xFF07101A),
    secondary = Color(0xFF3F8CFF),
    onSecondary = Color.White,
    background = Color(0xFF09111C),
    onBackground = Color(0xFFEDF5FB),
    surface = Color(0xD90F1B2A),
    onSurface = Color(0xFFEDF5FB),
    surfaceVariant = Color(0xD9122235),
    onSurfaceVariant = Color(0xFFB4C4D4),
    outline = Color(0x668EA2B8),
    error = Color(0xFFFF5D6C),
)

@Composable
fun MyfistTheme(content: @Composable () -> Unit) {
    MaterialTheme(colorScheme = MyfistColors, content = content)
}

@Composable
fun MyfistBackground(content: @Composable () -> Unit) {
    val portrait = remember {
        runCatching {
            val bytes = Base64.decode(BRUCE_CONTOUR_WEBP, Base64.DEFAULT)
            BitmapFactory.decodeByteArray(bytes, 0, bytes.size)?.asImageBitmap()
        }.getOrNull()
    }
    Box(modifier = Modifier.fillMaxSize().background(Color(0xFF09111C))) {
        portrait?.let { image ->
            Image(
                bitmap = image,
                contentDescription = null,
                modifier = Modifier.matchParentSize().alpha(0.24f),
                contentScale = ContentScale.Crop,
            )
        }
        Box(
            modifier = Modifier.matchParentSize().background(
                Brush.verticalGradient(
                    listOf(
                        Color(0xC907101A),
                        Color(0x990B1724),
                        Color(0xE609111C),
                    )
                )
            )
        )
        content()
    }
}

private const val BRUCE_CONTOUR_WEBP = "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"
