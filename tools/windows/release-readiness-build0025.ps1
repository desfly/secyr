$ErrorActionPreference = "Stop"
$Root = Resolve-Path "$PSScriptRoot\..\.."

python "$Root\tools\release_readiness_build0025.py"

Write-Host ""
Write-Host "All host release gates passed."
Write-Host "Next command:"
Write-Host "  .\tools\windows\build-build0022.ps1 -Clean"
Write-Host ""
Write-Warning "A real ESP-IDF build is still required before flashing."
