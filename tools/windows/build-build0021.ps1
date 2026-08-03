param(
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$Project = Resolve-Path "$PSScriptRoot\..\..\firmware\esp-idf"

& "$PSScriptRoot\check-esp-idf.ps1"

Push-Location $Project
try {
    if ($Clean) {
        idf.py fullclean
    }

    idf.py set-target esp32s3
    idf.py build

    $Release = Join-Path $Project "release-build0021"
    New-Item -ItemType Directory -Force $Release | Out-Null

    Copy-Item "build\bootloader\bootloader.bin" $Release -Force
    Copy-Item "build\partition_table\partition-table.bin" $Release -Force
    Copy-Item "build\homeguard_s3.bin" $Release -Force
    Copy-Item "build\ota_data_initial.bin" $Release -Force -ErrorAction SilentlyContinue
    Copy-Item "build\flasher_args.json" $Release -Force
    Copy-Item "build\flash_args" $Release -Force

    Get-FileHash "$Release\*" -Algorithm SHA256 |
        Format-Table -AutoSize |
        Out-String |
        Set-Content "$Release\SHA256SUMS.txt"

    Write-Host "Build completed: $Release"
} finally {
    Pop-Location
}
