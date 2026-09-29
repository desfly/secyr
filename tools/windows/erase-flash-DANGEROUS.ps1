param(
    [string]$Port = "COM4"
)

$ErrorActionPreference = "Stop"

Write-Warning "DANGEROUS FACTORY WIPE: THIS ERASES THE ENTIRE ESP32-S3 FLASH, INCLUDING NVS, WI-FI CREDENTIALS, COMMISSIONING, ACCESS DATA AND HARDWARE VERIFICATION."
$Answer = Read-Host "Type ERASE-ALL-NVS to continue"
if ($Answer -ne "ERASE-ALL-NVS") {
    Write-Host "Cancelled."
    exit 0
}

esptool.py --chip esp32s3 --port $Port erase_flash
