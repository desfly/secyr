$ErrorActionPreference = "Stop"

Write-Host "Checking ESP-IDF environment..."

if (-not $env:IDF_PATH) {
    throw "IDF_PATH is not set. Open 'ESP-IDF PowerShell' first."
}

$idf = Get-Command idf.py -ErrorAction SilentlyContinue
if (-not $idf) {
    throw "idf.py was not found in PATH."
}

Write-Host "IDF_PATH: $env:IDF_PATH"
idf.py --version
python --version

Write-Host "ESP-IDF environment OK."
