[CmdletBinding()]
param(
    [string]$GradleCommand = "",
    [string]$OutputDirectory = "artifacts\android",
    [switch]$NoBootstrap
)
$ErrorActionPreference = "Stop"
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$AndroidRoot = Join-Path $ProjectRoot "android"
$GradleVersion = "8.9"
$GradleSha256 = "d725d707bfabd4dfdc958c624003b3c80accc03f7037b5122c4b1d0ef15cecab"
$ToolsRoot = Join-Path $ProjectRoot ".tools"

if (-not $env:ANDROID_SDK_ROOT -and -not $env:ANDROID_HOME) {
    throw "ANDROID_SDK_ROOT or ANDROID_HOME is not set. Install Android SDK Platform 35 and Build Tools 34.0.0."
}

if (-not $GradleCommand) {
    $Existing = Get-Command gradle -ErrorAction SilentlyContinue
    if ($Existing) {
        $GradleCommand = $Existing.Source
    }
}

if (-not $GradleCommand) {
    if ($NoBootstrap) { throw "Gradle 8.9 is not available in PATH and bootstrap is disabled." }
    $Distribution = Join-Path $ToolsRoot "gradle-$GradleVersion-bin.zip"
    $InstallRoot = Join-Path $ToolsRoot "gradle-$GradleVersion"
    $GradleCommand = Join-Path $InstallRoot "bin\gradle.bat"
    New-Item -ItemType Directory -Force $ToolsRoot | Out-Null
    if (-not (Test-Path $GradleCommand)) {
        if (-not (Test-Path $Distribution)) {
            Write-Host "Downloading verified Gradle $GradleVersion..."
            Invoke-WebRequest -UseBasicParsing "https://services.gradle.org/distributions/gradle-$GradleVersion-bin.zip" -OutFile $Distribution
        }
        $Actual = (Get-FileHash -Algorithm SHA256 $Distribution).Hash.ToLowerInvariant()
        if ($Actual -ne $GradleSha256) {
            Remove-Item -Force $Distribution
            throw "Gradle archive SHA-256 mismatch. Expected $GradleSha256, got $Actual"
        }
        Expand-Archive -Force $Distribution $ToolsRoot
    }
}
if (-not (Test-Path $GradleCommand) -and -not (Get-Command $GradleCommand -ErrorAction SilentlyContinue)) {
    throw "Gradle command was not found: $GradleCommand"
}

Push-Location $AndroidRoot
try {
    & $GradleCommand --version
    if ($LASTEXITCODE -ne 0) { throw "Gradle startup failed" }
    & $GradleCommand --no-daemon --stacktrace testDebugUnitTest lintDebug assembleDebug
    if ($LASTEXITCODE -ne 0) { throw "Android Gradle build failed" }
    $Apk = Join-Path $AndroidRoot "app\build\outputs\apk\debug\app-debug.apk"
    if (-not (Test-Path $Apk)) { throw "APK was not created: $Apk" }
    $Destination = Join-Path $ProjectRoot $OutputDirectory
    New-Item -ItemType Directory -Force $Destination | Out-Null
    Copy-Item -Force $Apk (Join-Path $Destination "HomeGuard-S3-Build-0013-debug.apk")
    Get-ChildItem -File $Destination | Sort-Object Name | ForEach-Object {
        $Hash = Get-FileHash -Algorithm SHA256 $_.FullName
        "{0}  {1}" -f $Hash.Hash.ToLowerInvariant(), $_.Name
    } | Set-Content -Encoding ascii (Join-Path $Destination "SHA256SUMS.txt")
    Write-Host "Android artifacts: $Destination"
}
finally { Pop-Location }
