# Builds the customer package dist\NodeStroke (and dist\NodeStroke.zip) from the Release build:
# the plugin DLL, its res folder and README.txt. Run from the repository root after
#   cmake --build build-c4d --config Release --target nodestroke
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root 'build-c4d\bin\Release\plugins\nodestroke'
$dist = Join-Path $root 'dist'
$package = Join-Path $dist 'NodeStroke'

if (-not (Test-Path (Join-Path $build 'nodestroke.xdl64'))) { throw "No Release build at $build" }
if (Test-Path $package) { Remove-Item $package -Recurse -Force }
New-Item -ItemType Directory -Force $package | Out-Null

Copy-Item (Join-Path $build 'nodestroke.xdl64') $package
Copy-Item (Join-Path $build 'res') $package -Recurse
Copy-Item (Join-Path $root 'docs\package\README.txt') $package

$zip = Join-Path $dist 'NodeStroke.zip'
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path $package -DestinationPath $zip
Get-ChildItem $package -Recurse -File | ForEach-Object { $_.FullName.Substring($package.Length + 1) }
"package: $zip"
