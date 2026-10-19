param(
    [string]$ToolchainBin = 'C:\msys64\clang64\bin'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectRoot 'out\launchers'
$compiler = Join-Path $ToolchainBin 'clang++.exe'
$resourceCompiler = Join-Path $ToolchainBin 'windres.exe'

foreach ($tool in @($compiler, $resourceCompiler)) {
    if (-not (Test-Path -LiteralPath $tool)) {
        throw "No se encontro la herramienta: $tool"
    }
}

New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
Push-Location $projectRoot
try {
    $resource = Join-Path $buildDir 'launcher-res.o'
    & $resourceCompiler 'p3d_launcher.rc' '-O' 'coff' '-o' $resource
    if ($LASTEXITCODE -ne 0) { throw 'No se pudo compilar el icono del launcher.' }

    # Link the compiler runtime statically: double-clicking must not depend on
    # MSYS2 DLLs or a compiler directory in the user's PATH.
    $commonArgs = @('-std=c++17', '-O2', '-static', '-municode', '-mwindows',
                    'p3d_launcher.cpp', $resource, '-lshell32')
    & $compiler @commonArgs '-o' (Join-Path $buildDir 'p3d_launcher.exe')
    if ($LASTEXITCODE -ne 0) { throw 'No se pudo compilar p3d_launcher.exe.' }
    & $compiler @commonArgs '-DP3D_DIRECT_LAUNCH' '-o' (Join-Path $buildDir 'Jugar.exe')
    if ($LASTEXITCODE -ne 0) { throw 'No se pudo compilar Jugar.exe.' }

    $backupDir = Join-Path $projectRoot ('out\launcher-backups\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    foreach ($name in @('p3d_launcher.exe', 'Jugar.exe')) {
        $destination = Join-Path $projectRoot $name
        if (Test-Path -LiteralPath $destination) {
            New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
            Copy-Item -LiteralPath $destination -Destination (Join-Path $backupDir $name)
        }
        Copy-Item -LiteralPath (Join-Path $buildDir $name) -Destination $destination -Force
        Write-Output "Listo: $destination"
    }
}
finally {
    Pop-Location
}
