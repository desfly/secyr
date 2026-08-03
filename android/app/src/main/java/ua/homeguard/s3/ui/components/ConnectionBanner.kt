package ua.homeguard.s3.ui.components
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import ua.homeguard.s3.model.ControlPath
import ua.homeguard.s3.model.Transport
@Composable fun ConnectionBanner(connected:Boolean,transport:Transport,queued:Int,path:ControlPath=ControlPath.OFFLINE){
 val label=when(path){ControlPath.LOCAL->"Локальне з’єднання";ControlPath.LAST_KNOWN_LOCAL->"Остання локальна адреса";ControlPath.CLOUD->"Через інтернет";ControlPath.OFFLINE->"Пристрій офлайн"}
 AssistChip(onClick={},label={Text(if(connected)"$label • ${transport.name} • черга $queued" else "$label • команди будуть у черзі")})
}
