#!/usr/bin/env python3
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
kotlinc = shutil.which("kotlinc")
java = shutil.which("java")
kotlin_home = Path(kotlinc).resolve().parents[1] if kotlinc else None
coroutines = kotlin_home / "lib/kotlinx-coroutines-core-jvm.jar" if kotlin_home else Path()
if not kotlinc or not java or not coroutines.is_file():
    raise SystemExit(f"kotlinc, java and kotlinx-coroutines-core-jvm.jar are required; kotlinc={kotlinc}, coroutines={coroutines}")

with tempfile.TemporaryDirectory(prefix="homeguard-kotlin-") as temporary:
    temp = Path(temporary)
    handoff_sources = [
        root / "android/app/src/main/java/ua/homeguard/s3/provisioning/ProvisioningHandoff.kt",
        root / "android/app/src/main/java/ua/homeguard/s3/network/LegacyApiContract.kt",
        root / "android/app/src/main/java/ua/homeguard/s3/model/ConnectivityModels.kt",
        root / "android/app/src/main/java/ua/homeguard/s3/model/SystemModels.kt",
        root / "android/app/src/main/java/ua/homeguard/s3/network/EndpointSelection.kt",
        root / "android/pure-tests/ProvisioningHandoffTest.kt",
    ]
    handoff_jar = temp / "handoff-tests.jar"
    subprocess.run([kotlinc, "-J-Dkotlin.daemon.enabled=false", "-J-Dkotlin.compiler.execution.strategy=in-process", *map(str, handoff_sources), "-include-runtime", "-d", str(handoff_jar)], check=True)
    handoff_run = subprocess.run([java, "-jar", str(handoff_jar)], text=True, capture_output=True)
    if handoff_run.returncode != 0:
        raise SystemExit(f"handoff tests failed ({handoff_run.returncode}):\n{(handoff_run.stderr or handoff_run.stdout).strip()}")
    handoff_output = handoff_run.stdout.strip()


counts = []
for output in (handoff_output,):
    match = re.search(r"(\d+) tests PASS$", output)
    if not match:
        raise SystemExit(f"unexpected Kotlin test output: {output}")
    counts.append(int(match.group(1)))
print(f"HomeGuard Android portable: {sum(counts)} tests PASS")
