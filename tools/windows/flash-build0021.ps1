param(
    [string]$Port = "COM6",
    [int]$Baud = 460800
)

$ErrorActionPreference = "Stop"
$Project = Resolve-Path "$PSScriptRoot\..\..\firmware\esp-idf"

& "$PSScriptRoot\check-esp-idf.ps1"

Write-Host "SAFE FLASH: application/bootloader/partition images will be updated; NVS is preserved."
Write-Host "NO erase-flash / erase_flash is executed by this script."

Push-Location $Project
try {
    idf.py -p $Port -b $Baud flash
    idf.py -p $Port monitor
} finally {
    Pop-Location
}
