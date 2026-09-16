# exports tracked source as a zip for nexus's "source code" upload slot.
# doesn't bundle the commonlibsse-ng submodule - it's pinned in .gitmodules and pulled
# separately (`git submodule update --init --recursive`), no point duplicating 500+ files
# that already live on its own github repo.
param(
    [string]$OutDir = "$PSScriptRoot\..\dist"
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path "$PSScriptRoot\.."
$projectName = "SnapOpen"

$cmakeLists = Get-Content "$root\CMakeLists.txt" -Raw
if ($cmakeLists -notmatch 'VERSION\s+(\d+\.\d+\.\d+)') {
    throw "couldn't find project version in CMakeLists.txt"
}
$version = $matches[1]

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$zipPath = "$OutDir\$projectName-$version-SourceCode.zip"
if (Test-Path $zipPath) { Remove-Item -Force $zipPath }

Push-Location $root
try {
    git archive --format=zip --output "$zipPath" HEAD
    if ($LASTEXITCODE -ne 0) { throw "git archive failed" }
} finally {
    Pop-Location
}

Write-Host "source export: $zipPath"
