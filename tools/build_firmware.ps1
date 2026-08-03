[CmdletBinding()]
param(
    [string]$EspIdfPath = $env:IDF_PATH,
    [string]$OutputDirectory = "artifacts\firmware"
)
$ErrorActionPreference = "Stop"
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$FirmwareRoot = Join-Path $ProjectRoot "firmware\esp-idf"

if (-not $EspIdfPath) {
    throw "IDF_PATH is not set. Install ESP-IDF v5.4.4 and open an ESP-IDF PowerShell terminal."
}
$idf = Join-Path $EspIdfPath "tools\idf.py"
if (-not (Test-Path $idf)) { throw "idf.py was not found at $idf" }

Push-Location $FirmwareRoot
try {
    & python $idf set-target esp32s3
    if ($LASTEXITCODE -ne 0) { throw "idf.py set-target failed" }
    & python $idf reconfigure
    if ($LASTEXITCODE -ne 0) { throw "idf.py reconfigure failed" }
    if (-not (Test-Path "dependencies.lock")) { throw "ESP-IDF Component Manager did not create dependencies.lock" }
    & python $idf -D SDKCONFIG_DEFAULTS=sdkconfig.defaults build
    if ($LASTEXITCODE -ne 0) { throw "ESP-IDF build failed" }
    & python $idf size | Out-File -Encoding utf8 (Join-Path "build" "size-report.txt")

    $Destination = Join-Path $ProjectRoot $OutputDirectory
    New-Item -ItemType Directory -Force $Destination | Out-Null
    $Files = @{
        "build\homeguard_s3.bin" = "homeguard_s3.bin"
        "build\homeguard_s3.elf" = "homeguard_s3.elf"
        "build\homeguard_s3.map" = "homeguard_s3.map"
        "build\bootloader\bootloader.bin" = "bootloader.bin"
        "build\partition_table\partition-table.bin" = "partition-table.bin"
        "build\flash_args" = "flash_args"
        "build\flasher_args.json" = "flasher_args.json"
        "build\project_description.json" = "project_description.json"
        "build\size-report.txt" = "size-report.txt"
        "dependencies.lock" = "dependencies.lock"
        "main\idf_component.yml" = "idf_component.yml"
        "sdkconfig" = "sdkconfig"
    }
    foreach ($Entry in $Files.GetEnumerator()) {
        $Source = Join-Path $FirmwareRoot $Entry.Key
        if (-not (Test-Path $Source)) { throw "Expected artifact is missing: $($Entry.Key)" }
        Copy-Item -Force $Source (Join-Path $Destination $Entry.Value)
    }
    Get-ChildItem -File $Destination | Where-Object Name -ne "SHA256SUMS.txt" | Sort-Object Name | ForEach-Object {
        $Hash = Get-FileHash -Algorithm SHA256 $_.FullName
        "{0}  {1}" -f $Hash.Hash.ToLowerInvariant(), $_.Name
    } | Set-Content -Encoding ascii (Join-Path $Destination "SHA256SUMS.txt")
    Write-Host "Firmware artifacts: $Destination"
}
finally { Pop-Location }
