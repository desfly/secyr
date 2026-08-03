$ErrorActionPreference = "Stop"
$Root = Resolve-Path "$PSScriptRoot\..\.."

$Required = @(
    ".github\workflows\homeguard-build.yml",
    "firmware\esp-idf\CMakeLists.txt",
    "firmware\esp-idf\sdkconfig.defaults",
    "firmware\esp-idf\partitions.csv",
    "firmware\esp-idf\main\app_main.cpp",
    "tools\preflight_build0022.py",
    "web\index.html",
    "android\settings.gradle.kts"
)

foreach ($Relative in $Required) {
    $Path = Join-Path $Root $Relative
    if (-not (Test-Path $Path)) {
        throw "Відсутній файл: $Relative"
    }
}

python "$Root\tools\preflight_build0022.py"
python "$Root\tools\audit_esp_idf_sources.py"
python "$Root\tools\audit_component_dependencies.py"

Write-Host "GitHub package Build-0029: PASS"
