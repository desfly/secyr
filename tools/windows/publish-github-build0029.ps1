param(
    [Parameter(Mandatory = $true)]
    [string]$RepositoryUrl,

    [string]$Branch = "main"
)

$ErrorActionPreference = "Stop"
$Root = Resolve-Path "$PSScriptRoot\..\.."

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "Git не встановлений або недоступний у PATH."
}

Push-Location $Root
try {
    if (-not (Test-Path ".git")) {
        git init
    }

    git branch -M $Branch

    $remotes = git remote
    if ($remotes -contains "origin") {
        git remote set-url origin $RepositoryUrl
    } else {
        git remote add origin $RepositoryUrl
    }

    git add -A

    $status = git status --porcelain
    if ($status) {
        git commit -m "HomeGuard-S3 GitHub Build-0029"
    }

    git push -u origin $Branch
    Write-Host "Репозиторій завантажено: $RepositoryUrl"
} finally {
    Pop-Location
}
