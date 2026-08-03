param(
    [Parameter(Mandatory = $true)]
    [string]$Repository,

    [string]$Destination = ".\HomeGuard-S3-Build-0023-diagnostics"
)

$ErrorActionPreference = "Stop"

if (Test-Path $Destination) {
    Remove-Item $Destination -Recurse -Force
}

New-Item -ItemType Directory -Force $Destination | Out-Null

gh run download `
    --repo $Repository `
    --name "HomeGuard-S3-Build-0023-diagnostics" `
    --dir $Destination

Write-Host "Diagnostics downloaded to: $Destination"
