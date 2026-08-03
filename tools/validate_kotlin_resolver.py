#!/usr/bin/env python3
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
kotlinc = shutil.which('kotlinc')
if not kotlinc:
    raise SystemExit('kotlinc is required')

with tempfile.TemporaryDirectory(prefix='homeguard-resolver-') as temporary:
    tmp = Path(temporary)
    (tmp / 'CoroutineStubs.kt').write_text(r'''
package kotlinx.coroutines
class CoroutineScope
''', encoding='utf-8')
    (tmp / 'FlowStubs.kt').write_text(r'''
package kotlinx.coroutines.flow
import kotlinx.coroutines.CoroutineScope
interface Flow<T>
interface StateFlow<T> : Flow<T>
class SharingStarted private constructor() { companion object { val Eagerly = SharingStarted() } }
fun <A, B, R> combine(first: StateFlow<A>, second: StateFlow<B>, transform: (A, B) -> R): Flow<R> = error("compile-only")
fun <T> Flow<T>.stateIn(scope: CoroutineScope, started: SharingStarted, initialValue: T): StateFlow<T> = error("compile-only")
''', encoding='utf-8')
    (tmp / 'StorageStub.kt').write_text(r'''
package ua.homeguard.s3.storage
import kotlinx.coroutines.flow.StateFlow
class SettingsStore(val settings: StateFlow<AppSettings>)
''', encoding='utf-8')
    (tmp / 'DiscoveryStub.kt').write_text(r'''
package ua.homeguard.s3.network
import kotlinx.coroutines.flow.StateFlow
import ua.homeguard.s3.model.DiscoveredDevice
class LocalDiscoveryCoordinator(val devices: StateFlow<List<DiscoveredDevice>>)
''', encoding='utf-8')
    sources = [
        tmp / 'CoroutineStubs.kt',
        tmp / 'FlowStubs.kt',
        tmp / 'StorageStub.kt',
        tmp / 'DiscoveryStub.kt',
        root / 'android/app/src/main/java/ua/homeguard/s3/model/SystemModels.kt',
        root / 'android/app/src/main/java/ua/homeguard/s3/model/ConnectivityModels.kt',
        root / 'android/app/src/main/java/ua/homeguard/s3/storage/AppSettings.kt',
        root / 'android/app/src/main/java/ua/homeguard/s3/network/LocalApiContract.kt',
        root / 'android/app/src/main/java/ua/homeguard/s3/network/EndpointSelection.kt',
        root / 'android/app/src/main/java/ua/homeguard/s3/network/DeviceEndpointResolver.kt',
    ]
    jar = tmp / 'resolver-check.jar'
    subprocess.run([kotlinc, "-J-Dkotlin.daemon.enabled=false", "-J-Dkotlin.compiler.execution.strategy=in-process", *map(str, sources), '-d', str(jar)], check=True)
print('HomeGuard Android resolver: compile PASS')
