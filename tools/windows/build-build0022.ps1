param(
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$Root = Resolve-Path "$PSScriptRoot\..\.."
$Project = Join-Path $Root "firmware\esp-idf"
$Release = Join-Path $Project "release-build0022"

& "$PSScriptRoot\check-esp-idf.ps1"
python "$Root\tools\preflight_build0022.py"

Push-Location $Project
try {
    if ($Clean) {
        idf.py fullclean
    }

    idf.py set-target esp32s3
    idf.py reconfigure
    idf.py build

    New-Item -ItemType Directory -Force $Release | Out-Null

    Copy-Item "build\bootloader\bootloader.bin" $Release -Force
    Copy-Item "build\partition_table\partition-table.bin" $Release -Force
    Copy-Item "build\homeguard_s3.bin" $Release -Force
    Copy-Item "build\flasher_args.json" $Release -Force
    Copy-Item "build\flash_args" $Release -Force

    if (Test-Path "build\ota_data_initial.bin") {
        Copy-Item "build\ota_data_initial.bin" $Release -Force
    }

    Get-FileHash "$Release\*" -Algorithm SHA256 |
        ForEach-Object {
            "$($_.Hash.ToLower())  $([IO.Path]::GetFileName($_.Path))"
        } |
        Set-Content "$Release\SHA256SUMS.txt"

    python "$Root\tools\generate_firmware_manifest.py" `
        $Release `
        "$Release\manifest.json"

    Write-Host "Firmware release created: $Release"
} finally {
    Pop-Location
}
