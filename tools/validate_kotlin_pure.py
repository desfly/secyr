#!/usr/bin/env python3
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
kotlinc = shutil.which("kotlinc")
java = shutil.which("java")
coroutines = Path("/root/.sdkman/candidates/kotlin/1.9.0/lib/kotlinx-coroutines-core-jvm.jar")
if not kotlinc or not java or not coroutines.is_file():
    raise SystemExit("kotlinc, java and kotlinx-coroutines-core-jvm.jar are required")

with tempfile.TemporaryDirectory(prefix="homeguard-kotlin-") as temporary:
    temp = Path(temporary)
    handoff_sources = [
        root / "android/app/src/main/java/ua/homeguard/s3/provisioning/ProvisioningHandoff.kt",
        root / "android/app/src/main/java/ua/homeguard/s3/network/LocalApiContract.kt",
        root / "android/app/src/main/java/ua/homeguard/s3/model/ConnectivityModels.kt",
        root / "android/app/src/main/java/ua/homeguard/s3/model/SystemModels.kt",
        root / "android/app/src/main/java/ua/homeguard/s3/network/EndpointSelection.kt",
        root / "android/pure-tests/ProvisioningHandoffTest.kt",
    ]
    handoff_jar = temp / "handoff-tests.jar"
    subprocess.run([kotlinc, "-J-Dkotlin.daemon.enabled=false", "-J-Dkotlin.compiler.execution.strategy=in-process", *map(str, handoff_sources), "-include-runtime", "-d", str(handoff_jar)], check=True)
    handoff_output = subprocess.run([java, "-jar", str(handoff_jar)], check=True, text=True, capture_output=True).stdout.strip()

    queue_sources = [
        root / "android/app/src/main/java/ua/homeguard/s3/model/CommandModels.kt",
        root / "android/app/src/main/java/ua/homeguard/s3/queue/QueuedCommand.kt",
        root / "android/app/src/main/java/ua/homeguard/s3/queue/OfflineCommandQueue.kt",
        root / "android/pure-tests/QueueRuntimeTest.kt",
    ]
    queue_jar = temp / "queue-tests.jar"
    subprocess.run([kotlinc, "-J-Dkotlin.daemon.enabled=false", "-J-Dkotlin.compiler.execution.strategy=in-process", "-classpath", str(coroutines), *map(str, queue_sources), "-include-runtime", "-d", str(queue_jar)], check=True)
    queue_output = subprocess.run([java, "-cp", f"{queue_jar}:{coroutines}", "QueueRuntimeTestKt"], check=True, text=True, capture_output=True).stdout.strip()

counts = []
for output in (handoff_output, queue_output):
    match = re.search(r"(\d+) tests PASS$", output)
    if not match:
        raise SystemExit(f"unexpected Kotlin test output: {output}")
    counts.append(int(match.group(1)))
print(f"HomeGuard Android portable: {sum(counts)} tests PASS")
