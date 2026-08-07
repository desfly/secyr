package ua.homeguard.s3

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import com.journeyapps.barcodescanner.ScanContract
import com.journeyapps.barcodescanner.ScanOptions
import ua.homeguard.s3.model.ProvisioningPhase
import ua.homeguard.s3.model.SystemSnapshot
import ua.homeguard.s3.network.DeviceEndpointResolver
import ua.homeguard.s3.network.DeviceSession
import ua.homeguard.s3.network.LocalDiscoveryCoordinator
import ua.homeguard.s3.network.TelemetrySocket
import ua.homeguard.s3.repository.ProvisioningCoordinator
import ua.homeguard.s3.storage.SettingsStore
import ua.homeguard.s3.ui.screens.DashboardScreen
import ua.homeguard.s3.ui.screens.ProvisioningScreen

class MainActivity : ComponentActivity() {
    private lateinit var discovery: LocalDiscoveryCoordinator
    private lateinit var settings: SettingsStore
    private lateinit var resolver: DeviceEndpointResolver
    private lateinit var provisioning: ProvisioningCoordinator
    private lateinit var telemetry: TelemetrySocket
    private lateinit var session: DeviceSession

    private val qrScanner = registerForActivityResult(ScanContract()) { result ->
        result.contents?.let(provisioning::acceptQr)
    }

    private val permissionLauncher = registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {
        if (requiredProvisioningPermissions().all { permission ->
                ContextCompat.checkSelfPermission(this, permission) == PackageManager.PERMISSION_GRANTED
            }) launchQrScanner()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        settings = SettingsStore(this)
        discovery = LocalDiscoveryCoordinator(this, lifecycleScope)
        resolver = DeviceEndpointResolver(settings, discovery, lifecycleScope)
        provisioning = ProvisioningCoordinator(this, settings, discovery, lifecycleScope)
        telemetry = TelemetrySocket()
        session = DeviceSession(lifecycleScope, resolver.endpoint, settings, telemetry)
        discovery.start()
        session.start()
        setContent {
            val appSettings by settings.settings.collectAsState()
            val devices by discovery.devices.collectAsState()
            val endpoint by resolver.endpoint.collectAsState()
            val provisioningState by provisioning.state.collectAsState()
            val snapshot by telemetry.snapshots().collectAsState(initial = SystemSnapshot())
            MaterialTheme {
                val provisioningActive = provisioningState.phase in setOf(
                    ProvisioningPhase.CONNECTING_SETUP_AP,
                    ProvisioningPhase.AUTHORIZING,
                    ProvisioningPhase.APPLYING,
                    ProvisioningPhase.WAITING_FOR_RESTART,
                    ProvisioningPhase.DISCOVERING_LOCAL
                )
                if (appSettings.deviceId.isBlank() || provisioningActive) {
                    ProvisioningScreen(
                        state = provisioningState,
                        onScan = ::requestQrScan,
                        onProvision = provisioning::provision
                    )
                } else {
                    DashboardScreen(
                        versionName = BuildConfig.VERSION_NAME,
                        localDevices = devices.size,
                        route = endpoint.path.name,
                        deviceId = appSettings.deviceId,
                        snapshot = snapshot,
                    )
                }
            }
        }
    }

    private fun requestQrScan() {
        val missing = requiredProvisioningPermissions().filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }
        if (missing.isEmpty()) launchQrScanner() else permissionLauncher.launch(missing.toTypedArray())
    }

    private fun launchQrScanner() {
        qrScanner.launch(
            ScanOptions().setPrompt("Скануйте QR HomeGuard-S3")
                .setBeepEnabled(false).setOrientationLocked(false)
        )
    }

    private fun requiredProvisioningPermissions(): List<String> = buildList {
        add(Manifest.permission.CAMERA)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) add(Manifest.permission.NEARBY_WIFI_DEVICES)
        else add(Manifest.permission.ACCESS_FINE_LOCATION)
    }

    override fun onDestroy() {
        session.stop()
        discovery.stop()
        super.onDestroy()
    }
}
