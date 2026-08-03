param(
    [Parameter(Mandatory = $true)]
    [string]$RepositoryUrl,

    [string]$Branch = "main",

    [switch]$Force
)

$ErrorActionPreference = "Stop"

$Root = Resolve-Path "$PSScriptRoot\..\.."
$Temp = Join-Path $env:TEMP "homeguard-build0023-publish"

if (Test-Path $Temp) {
    Remove-Item $Temp -Recurse -Force
}

git clone $RepositoryUrl $Temp
Push-Location $Temp

try {
    git checkout -B $Branch

    Get-ChildItem -Force |
        Where-Object { $_.Name -ne ".git" } |
        Remove-Item -Recurse -Force

    Copy-Item "$Root\*" $Temp -Recurse -Force
    Copy-Item "$Root\.github" $Temp -Recurse -Force

    git add -A

    if (-not (git status --porcelain)) {
        Write-Host "No changes to commit."
        exit 0
    }

    git commit -m "HomeGuard-S3 Build-0023"

    if ($Force) {
        git push --force-with-lease origin $Branch
    } else {
        git push origin $Branch
    }

    Write-Host "Build-0023 published to $RepositoryUrl"
} finally {
    Pop-Location
}
