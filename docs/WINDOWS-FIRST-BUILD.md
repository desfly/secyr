# First firmware build on Windows

Install Espressif ESP-IDF 5.4.2 with the official Windows installer.

Open **ESP-IDF 5.4 PowerShell**, then:

```powershell
cd <project>
.\tools\windows\check-esp-idf.ps1
.\tools\windows\build-build0021.ps1 -Clean
```

Expected release directory:

```text
firmware\esp-idf\release-build0021\
```

Expected files:

```text
bootloader.bin
partition-table.bin
homeguard_s3.bin
flasher_args.json
flash_args
SHA256SUMS.txt
```

Flash only after a successful build:

```powershell
.\tools\windows\flash-build0021.ps1 -Port COM4
```

The erase script is intentionally separate and requires typing `ERASE`.
Normal firmware testing must not begin with a full-chip erase.
