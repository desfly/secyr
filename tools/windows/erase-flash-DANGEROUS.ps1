param(
    [string]$Port = "COM4"
)

$ErrorActionPreference = "Stop"

Write-Warning "THIS ERASES THE ENTIRE ESP32-S3 FLASH."
$Answer = Read-Host "Type ERASE to continue"
if ($Answer -ne "ERASE") {
    Write-Host "Cancelled."
    exit 0
}

esptool.py --chip esp32s3 --port $Port erase_flash
