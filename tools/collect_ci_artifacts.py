#!/usr/bin/env python3
"""Validate and normalize CI-produced firmware/APK artifacts."""
from __future__ import annotations
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import sys

EXPECTED_FIRMWARE = (
    "homeguard_s3.bin",
    "homeguard_s3.elf",
    "homeguard_s3.map",
    "bootloader.bin",
    "partition-table.bin",
    "flash_args",
    "flasher_args.json",
    "dependencies.lock",
    "idf_component.yml",
    "project_description.json",
    "sdkconfig",
    "size-report.txt",
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--firmware", type=Path)
    parser.add_argument("--apk", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    report: dict[str, object] = {"firmware": {}, "android": {}}

    if args.firmware:
        missing = [name for name in EXPECTED_FIRMWARE if not (args.firmware / name).is_file()]
        if missing:
            raise SystemExit(f"Missing firmware artifacts: {', '.join(missing)}")
        target = args.out / "firmware"
        target.mkdir(exist_ok=True)
        for name in EXPECTED_FIRMWARE:
            source = args.firmware / name
            destination = target / name
            shutil.copy2(source, destination)
            report["firmware"][name] = {"bytes": destination.stat().st_size, "sha256": sha256(destination)}

    if args.apk:
        if not args.apk.is_file() or args.apk.suffix.lower() != ".apk":
            raise SystemExit("--apk must point to a built APK")
        target = args.out / "android"
        target.mkdir(exist_ok=True)
        destination = target / "HomeGuard-S3-Build-0013-debug.apk"
        shutil.copy2(args.apk, destination)
        report["android"][destination.name] = {"bytes": destination.stat().st_size, "sha256": sha256(destination)}

    if not report["firmware"] and not report["android"]:
        raise SystemExit("No artifacts supplied")
    (args.out / "ARTIFACTS.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
