# Builds NodeStroke for each Cinema 4D version and packages it as
# dist\NodeStroke-C4D<version>.zip. Every zip holds one NodeStroke folder with
# the plugin, its res folder and that version's README.txt.
#   .\tools\build-release.ps1                  # 2027 and 2026
#   .\tools\build-release.ps1 -Versions 2026   # one version
#   .\tools\build-release.ps1 -SkipBuild       # package the existing builds
[CmdletBinding()]
param(
    [ValidateSet(2026, 2027)][int[]]$Versions = @(2027, 2026),
    [switch]$SkipBuild,
    [string]$Generator,
    [string]$PythonExecutable,
    [ValidateRange(1, 64)][int]$Jobs = 8
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$dist = Join-Path $root 'dist'

# Each version builds with exactly one SDK release into its own build folder, so
# no CMake cache is ever shared between SDKs. The 2026 build must use the 2026.0.0
# SDK: newer 2026 SDKs (such as the one in a 2026.4 installation) raise the ABI.
$targets = @{
    2027 = @{ Sdk = 'sdk'; Api = 2027000; Abi = $null; Build = 'build-c4d'; Readme = 'docs\package\README.txt' }
    2026 = @{ Sdk = 'sdk-2026.0.0'; Api = 2026000; Abi = 2026000; Build = 'build-c4d-2026.0.0'; Readme = 'docs\package\README-2026.txt' }
}

function Assert-Sdk([int]$Version, [hashtable]$Target, [string]$Sdk) {
    $apiHeader = Join-Path $Sdk 'frameworks\cinema.framework\source\operatingsystem.h'
    $abiHeader = Join-Path $Sdk 'frameworks\core.framework\source\maxon\apibase_version.h'
    if (-not (Test-Path -LiteralPath $apiHeader -PathType Leaf)) { throw "Cinema 4D $Version SDK not found at $Sdk" }
    if ((Get-Content -LiteralPath $apiHeader -Raw) -notmatch "(?m)^#define\s+API_VERSION\s+$($Target.Api)\s*$") {
        throw "Cinema 4D $Version needs an SDK with API_VERSION $($Target.Api): $Sdk"
    }
    if ($Target.Abi -and (Get-Content -LiteralPath $abiHeader -Raw) -notmatch "(?m)^#define\s+MAXON_API_ABI_VERSION\s+$($Target.Abi)\s*$") {
        throw "Cinema 4D $Version needs an SDK with MAXON_API_ABI_VERSION $($Target.Abi): $Sdk"
    }
}

# A running Cinema 4D keeps the plugin's PDB open, which fails the link step.
# Renaming a locked file is allowed, so move it aside.
function Move-LockedPdb([string]$Build) {
    $pdbs = Get-ChildItem -LiteralPath $Build -Recurse -Filter 'nodestroke*.pdb' -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -notlike '*.locked-*' }
    foreach ($pdb in $pdbs) {
        try { [IO.File]::Open($pdb.FullName, 'Open', 'ReadWrite', 'None').Close() }
        catch { Rename-Item -LiteralPath $pdb.FullName "$($pdb.BaseName).locked-$(Get-Date -Format HHmmss).pdb" }
    }
}

function Invoke-Build([int]$Version, [hashtable]$Target, [string]$Sdk, [string]$Build) {
    $generator = $Generator
    if (-not $generator) {
        $generator = 'Visual Studio 17 2022'
        $cache = Join-Path $Build 'CMakeCache.txt'
        if (Test-Path -LiteralPath $cache -PathType Leaf) {
            $cached = Select-String -LiteralPath $cache -Pattern '^CMAKE_GENERATOR:INTERNAL=(.+)$'
            if ($cached) { $generator = $cached.Matches[0].Groups[1].Value }
        }
    }
    $configure = @('-S', $Sdk, '-B', $Build, '-G', $generator, '-A', 'x64', "-DMAXON_SDK_MODULES_DIR=$root/c4d_plugin")
    if ($PythonExecutable) { $configure += "-DPython3_EXECUTABLE=$PythonExecutable" }
    & cmake @configure | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Cinema 4D $Version CMake configure failed: $LASTEXITCODE" }
    Move-LockedPdb $Build
    & cmake --build $Build --config Release --target nodestroke --parallel $Jobs | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Cinema 4D $Version build failed: $LASTEXITCODE" }
}

function New-Package([int]$Version, [hashtable]$Target, [string]$Build) {
    $output = Join-Path $Build 'bin\Release\plugins\nodestroke'
    $binary = Join-Path $output 'nodestroke.xdl64'
    $readme = Join-Path $root $Target.Readme
    if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) { throw "No Cinema 4D $Version build at $output" }
    if (-not (Test-Path -LiteralPath (Join-Path $output 'res') -PathType Container)) { throw "No resources at $output" }
    if (-not (Test-Path -LiteralPath $readme -PathType Leaf)) { throw "Missing README: $readme" }

    # Staged as dist\C4D<version>\NodeStroke so the zip's top folder is NodeStroke.
    $stage = Join-Path $dist "C4D$Version"
    if (Test-Path -LiteralPath $stage) {
        if ((Get-Item -LiteralPath $stage).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "$stage must not be a junction or symlink." }
        Remove-Item -LiteralPath $stage -Recurse -Force
    }
    $package = Join-Path $stage 'NodeStroke'
    New-Item -ItemType Directory -Force $package | Out-Null
    Copy-Item -LiteralPath $binary -Destination $package
    Copy-Item -LiteralPath (Join-Path $output 'res') -Destination $package -Recurse
    Copy-Item -LiteralPath $readme -Destination (Join-Path $package 'README.txt')

    $hash = (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash
    if ((Get-FileHash -LiteralPath (Join-Path $package 'nodestroke.xdl64') -Algorithm SHA256).Hash -ne $hash) {
        throw "Cinema 4D $Version packaged plugin does not match the build output."
    }
    $zip = Join-Path $dist "NodeStroke-C4D$Version.zip"
    if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
    Compress-Archive -Path $package -DestinationPath $zip
    [pscustomobject]@{ Version = $Version; Zip = $zip; Sha256 = $hash }
}

$results = foreach ($version in ($Versions | Sort-Object -Unique -Descending)) {
    $target = $targets[$version]
    $build = Join-Path $root $target.Build
    if (-not $SkipBuild) {
        $sdk = Join-Path $root $target.Sdk
        Assert-Sdk $version $target $sdk
        Invoke-Build $version $target (Resolve-Path -LiteralPath $sdk).Path $build
    }
    New-Package $version $target $build
}
$results | Format-Table -AutoSize
