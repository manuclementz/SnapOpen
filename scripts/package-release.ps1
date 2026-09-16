# builds release preset, zips SKSE/Plugins/{dll,ini} for nexus upload
param(
    [string]$OutDir = "$PSScriptRoot\..\dist"
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path "$PSScriptRoot\.."
$projectName = "SnapOpen"
$projectReleaseName = "Snap Open - Instant doors and containers animations"

$cmakeLists = Get-Content "$root\CMakeLists.txt" -Raw
if ($cmakeLists -notmatch 'VERSION\s+(\d+\.\d+\.\d+)') {
    throw "couldn't find project version in CMakeLists.txt"
}
$version = $matches[1]

# needs the msvc dev environment, grab it via vswhere if we're not already in one
if (-not $env:VCINSTALLDIR) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsPath) {
        throw "no Visual Studio install with the C++ toolset found"
    }
    $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
    cmd /c "`"$vcvars`" && set" | ForEach-Object {
        if ($_ -match "^([^=]+)=(.*)$") {
            Set-Item -Path "Env:$($matches[1])" -Value $matches[2] -ErrorAction SilentlyContinue
        }
    }
}

Push-Location $root
try {
    cmake --preset release
    if ($LASTEXITCODE -ne 0) { throw "configure failed" }
    cmake --build --preset release
    if ($LASTEXITCODE -ne 0) { throw "build failed" }
} finally {
    Pop-Location
}

$dll = "$root\build\release\$projectName.dll"
if (-not (Test-Path $dll)) {
    throw "expected dll not found at $dll"
}

$stageDir = "$OutDir\$projectName-$version"
$pluginsDir = "$stageDir\SKSE\Plugins"
if (Test-Path $stageDir) { Remove-Item -Recurse -Force $stageDir }
New-Item -ItemType Directory -Force -Path $pluginsDir | Out-Null

Copy-Item $dll "$pluginsDir\$projectName.dll"
Copy-Item "$root\$projectName.ini" "$pluginsDir\$projectName.ini"

$zipPath = "$OutDir\$projectReleaseName-$version.zip"
if (Test-Path $zipPath) { Remove-Item -Force $zipPath }
Compress-Archive -Path "$stageDir\*" -DestinationPath $zipPath

Write-Host "release package: $zipPath"
