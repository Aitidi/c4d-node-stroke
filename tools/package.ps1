# Builds the customer package dist\NodeStroke (and dist\NodeStroke.zip) from the Release build:
# the plugin DLL, its res folder and README.txt. Run from the repository root after
#   cmake --build build-c4d --config Release --target nodestroke
[CmdletBinding()]
param(
  [string]$BuildDirectory = 'build-c4d',
  [ValidatePattern('^NodeStroke(?:-[A-Za-z0-9]+(?:[.-][A-Za-z0-9]+)*)?$')][string]$PackageName = 'NodeStroke',
  [string]$Readme = 'docs\package\README.txt'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (-not [IO.Path]::IsPathRooted($BuildDirectory)) { $BuildDirectory = Join-Path $root $BuildDirectory }
$build = Join-Path $BuildDirectory 'bin\Release\plugins\nodestroke'
if (-not [IO.Path]::IsPathRooted($Readme)) { $Readme = Join-Path $root $Readme }
$dist = Join-Path $root 'dist'
$package = [IO.Path]::GetFullPath((Join-Path $dist $PackageName))
$expectedParent = [IO.Path]::GetFullPath($dist)
if ((Split-Path -Parent $package) -ne $expectedParent) { throw 'Package must be directly inside dist.' }

if (-not (Test-Path (Join-Path $build 'nodestroke.xdl64'))) { throw "No Release build at $build" }
if (-not (Test-Path (Join-Path $build 'res') -PathType Container)) { throw "No resources at $build" }
if (-not (Test-Path -LiteralPath $Readme -PathType Leaf)) { throw "Missing README: $Readme" }
if (Test-Path -LiteralPath $package) {
  $existing = Get-Item -LiteralPath $package
  if ($existing.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Package directory must not be a junction or symlink.' }
  Remove-Item -LiteralPath $package -Recurse -Force
}
New-Item -ItemType Directory -Force $package | Out-Null

Copy-Item (Join-Path $build 'nodestroke.xdl64') $package
Copy-Item (Join-Path $build 'res') $package -Recurse
Copy-Item -LiteralPath $Readme -Destination (Join-Path $package 'README.txt')

$zip = Join-Path $dist "$PackageName.zip"
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
Compress-Archive -Path $package -DestinationPath $zip
Get-ChildItem $package -Recurse -File | ForEach-Object { $_.FullName.Substring($package.Length + 1) }
"package: $zip"
