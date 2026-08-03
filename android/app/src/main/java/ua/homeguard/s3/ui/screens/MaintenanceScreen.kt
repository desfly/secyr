package ua.homeguard.s3.ui.screens
import androidx.compose.foundation.layout.Column
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import ua.homeguard.s3.model.CommandType
@Composable fun MaintenanceScreen(active:Boolean,onCommand:(CommandType,Boolean)->Unit){Column{Text("Сервісний режим: ${if(active)"УВІМКНЕНО" else "вимкнено"}");Button({onCommand(if(active)CommandType.EXIT_MAINTENANCE else CommandType.ENTER_MAINTENANCE,active)}){Text(if(active)"Вийти" else "Увійти")}}}
