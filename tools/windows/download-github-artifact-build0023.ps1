param(
    [Parameter(Mandatory = $true)]
    [string]$Repository,

    [string]$ArtifactName = "HomeGuard-S3-Build-0023-firmware",

    [string]$Destination = ".\HomeGuard-S3-Build-0023-firmware"
)

$ErrorActionPreference = "Stop"

$gh = Get-Command gh -ErrorAction SilentlyContinue
if (-not $gh) {
    throw "GitHub CLI 'gh' is not installed."
}

gh auth status

if (Test-Path $Destination) {
    Remove-Item $Destination -Recurse -Force
}

New-Item -ItemType Directory -Force $Destination | Out-Null

gh run download `
    --repo $Repository `
    --name $ArtifactName `
    --dir $Destination

Write-Host "Downloaded to: $Destination"

$Manifest = Join-Path $Destination "manifest.json"
if (Test-Path $Manifest) {
    Get-Content $Manifest
}

$Sums = Join-Path $Destination "SHA256SUMS.txt"
if (Test-Path $Sums) {
    Write-Host "SHA256:"
    Get-Content $Sums
}
