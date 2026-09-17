[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$CLionPath,
    [string]$BuildDirectory = 'out/m1-validation/english-debug'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$build = [IO.Path]::GetFullPath((Join-Path $root $BuildDirectory))
$ninja = Join-Path $CLionPath 'bin/ninja/win/x64/ninja.exe'
$header = Join-Path $root 'include/core/asset_paths.h'
$objects = @(
    'CMakeFiles/SymoCraft.dir/src/main.cpp.obj',
    'CMakeFiles/SymoCraft.dir/src/core/application.cpp.obj',
    'CMakeFiles/SymoCraft.dir/src/renderer/renderer.cpp.obj',
    'CMakeFiles/symocraft_assets.dir/src/core/asset_paths.cpp.obj',
    'tests/CMakeFiles/asset_paths_tests.dir/asset_paths_tests.cpp.obj'
)

$timestamps = @{}
foreach ($object in $objects) {
    $dependencies = & $ninja -C $build -t deps $object
    if ($LASTEXITCODE -ne 0 -or ($dependencies -join "`n") -notmatch 'asset_paths[.]h') {
        throw "Missing asset_paths.h dependency for $object."
    }
    Write-Output $dependencies[0]
    $timestamps[$object] = (Get-Item -LiteralPath (Join-Path $build $object)).LastWriteTimeUtc
}

$hash = (Get-FileHash -LiteralPath $header -Algorithm SHA256).Hash
$originalTime = (Get-Item -LiteralPath $header).LastWriteTimeUtc
try {
    # Change only the timestamp of this M1-owned header; never change its content.
    (Get-Item -LiteralPath $header).LastWriteTimeUtc = [DateTime]::UtcNow
    & (Join-Path $root 'scripts/build.ps1') -Configuration Debug -Action Build `
        -CLionPath $CLionPath -BuildDirectory $build
    foreach ($object in $objects) {
        $updated = (Get-Item -LiteralPath (Join-Path $build $object)).LastWriteTimeUtc
        if ($updated -le $timestamps[$object]) {
            throw "Header change did not rebuild $object."
        }
        $timestamps[$object] = $updated
        Write-Output "Recompiled: $object"
    }
} finally {
    if ((Get-FileHash -LiteralPath $header -Algorithm SHA256).Hash -eq $hash) {
        (Get-Item -LiteralPath $header).LastWriteTimeUtc = $originalTime
    } else {
        throw 'Header content changed during verification; leaving the new file untouched.'
    }
}

& (Join-Path $root 'scripts/build.ps1') -Configuration Debug -Action Build `
    -CLionPath $CLionPath -BuildDirectory $build
foreach ($object in $objects) {
    if ((Get-Item -LiteralPath (Join-Path $build $object)).LastWriteTimeUtc -ne $timestamps[$object]) {
        throw "No-change build unexpectedly recompiled $object."
    }
}
Write-Output 'PASS: five header dependents rebuilt; unchanged build performed no recompilation; header content preserved.'
