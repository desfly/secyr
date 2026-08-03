param(
    [string]$Port = "COM4",
    [int]$Baud = 460800
)

$ErrorActionPreference = "Stop"
$Project = Resolve-Path "$PSScriptRoot\..\..\firmware\esp-idf"

& "$PSScriptRoot\check-esp-idf.ps1"

Push-Location $Project
try {
    idf.py -p $Port -b $Baud flash
    idf.py -p $Port monitor
} finally {
    Pop-Location
}
