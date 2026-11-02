# Build with the 2026.0.0 SDK; never reuse a cache from another SDK release.
[CmdletBinding()]
param(
    [string]$SdkPath = 'sdk-2026.0.0',
    [string]$Generator,
    [string]$PythonExecutable,
    [ValidateRange(1, 64)][int]$Jobs = 8
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (-not [IO.Path]::IsPathRooted($SdkPath)) { $SdkPath = Join-Path $root $SdkPath }
$SdkPath = (Resolve-Path -LiteralPath $SdkPath).Path
$apiHeader = Join-Path $SdkPath 'frameworks\cinema.framework\source\operatingsystem.h'
$abiHeader = Join-Path $SdkPath 'frameworks\core.framework\source\maxon\apibase_version.h'
if ((Get-Content -LiteralPath $apiHeader -Raw) -notmatch '(?m)^#define\s+API_VERSION\s+2026000\s*$' -or
    (Get-Content -LiteralPath $abiHeader -Raw) -notmatch '(?m)^#define\s+MAXON_API_ABI_VERSION\s+2026000\s*$') {
    throw 'This build requires Cinema 4D SDK 2026.0.0 (API and ABI 2026000).'
}
$build = Join-Path $root 'build-c4d-2026.0.0'
if (-not $Generator) {
    $Generator = 'Visual Studio 17 2022'
    $cache = Join-Path $build 'CMakeCache.txt'
    if (Test-Path -LiteralPath $cache -PathType Leaf) {
        $cached = Select-String -LiteralPath $cache -Pattern '^CMAKE_GENERATOR:INTERNAL=(.+)$'
        if ($cached -and $cached.Matches[0].Groups[1].Value) {
            $Generator = $cached.Matches[0].Groups[1].Value
        }
    }
}
$configure = @('-S', $SdkPath, '-B', $build, '-G', $Generator, '-A', 'x64',
    "-DMAXON_SDK_MODULES_DIR=$root/c4d_plugin")
if ($PythonExecutable) { $configure += "-DPython3_EXECUTABLE=$PythonExecutable" }
& cmake @configure
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed: $LASTEXITCODE" }
& cmake --build $build --config Release --target nodestroke --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw "CMake build failed: $LASTEXITCODE" }
$binary = Join-Path $build 'bin\Release\plugins\nodestroke\nodestroke.xdl64'
if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) { throw "Missing plugin: $binary" }
Write-Output "Cinema 4D 2026.0.0 SDK build: $binary"
