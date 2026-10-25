# SPDX-FileCopyrightText: 2026 Petr Vanek
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds Sqliteman for 64-bit Windows with MSVC: a folder with the program
# and every DLL it needs, a zip of that folder, and an installer when Inno
# Setup is there. See docs/development/packaging.md for what has to be
# installed first.

#Requires -Version 7.3

param(
    # The Qt kit, e.g. C:\Qt\6.8.3\msvc2022_64.
    [Parameter(Mandatory = $true)] [string] $QtDir,
    [string] $WorkDir,
    [switch] $NoDocs,
    # signtool's /n argument; the program and the installer are signed with it.
    [string] $SignCertificate
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true

$SourceDir = (Resolve-Path "$PSScriptRoot\..\..").Path
if (-not $WorkDir) { $WorkDir = Join-Path $SourceDir 'build-windows' }
$Version = (Select-String -Path "$SourceDir\CMakeLists.txt" -Pattern '^\s*VERSION\s+([0-9.]+)\s*$' |
    Select-Object -First 1).Matches[0].Groups[1].Value

# The compiler and its environment, unless this already is a developer shell.
if (-not $env:VCToolsRedistDir) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) { throw 'Visual Studio with the C++ tools was not found.' }
    Import-Module "$vs\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
    Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
}

New-Item -ItemType Directory -Force $WorkDir | Out-Null
$env:PATH = "$QtDir\bin;$env:PATH"

if (-not $NoDocs) {
    Write-Host '== Sphinx'
    $venv = Join-Path $WorkDir 'venv'
    if (-not (Test-Path "$venv\Scripts\sphinx-build.exe")) {
        python -m venv $venv
        & "$venv\Scripts\pip.exe" install --quiet -r "$SourceDir\docs\requirements.txt"
    }
    $env:PATH = "$venv\Scripts;$env:PATH"
}

Write-Host "== Building $Version"
# SQLite and QScintilla are built into the program from pinned archives.
$Build = Join-Path $WorkDir 'build'
cmake -S $SourceDir -B $Build -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_PREFIX_PATH="$QtDir" `
    -DSQLITEMAN_BUNDLED_SQLITE=ON `
    -DSQLITEMAN_BUNDLED_QSCINTILLA=ON `
    -DSQLITEMAN_BUILD_TESTS=OFF `
    -DSQLITEMAN_WITH_DOCS="$(if ($NoDocs) { 'OFF' } else { 'ON' })"
cmake --build $Build
if (-not $NoDocs) { cmake --build $Build --target docs-qch }

$Stage = Join-Path $WorkDir 'Sqliteman'
if (Test-Path $Stage) { Remove-Item -Recurse -Force $Stage }
cmake --install $Build --prefix $Stage
$Exe = "$Stage\bin\sqliteman.exe"

Write-Host '== Bundling libraries'
# Qt's DLLs and plugins. The Help module brings Qt SQL, and with it the
# SQLite driver the help engine keeps its index in. The other drivers stay
# out: nothing uses them, and they need their databases' client libraries
# (Mimer's MIMAPI64.dll, ODBC).
windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw `
    --no-compiler-runtime --exclude-plugins qsqlmimer,qsqlodbc,qsqlpsql $Exe

# The C++ runtime goes beside the program rather than through the
# redistributable installer, so the zip runs on a bare system too. Before
# the next step, so that one takes these copies and not some other.
Copy-Item "$env:VCToolsRedistDir\x64\Microsoft.VC*.CRT\*.dll" "$Stage\bin"

cmake "-DEXECUTABLE=$Exe" "-DSEARCH_DIRS=$QtDir\bin" -P "$PSScriptRoot\deploy-dlls.cmake"

Copy-Item "$SourceDir\LICENSE" "$Stage\LICENSE.txt"

if ($SignCertificate) {
    Write-Host '== Signing'
    signtool sign /n $SignCertificate /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 $Exe
}

Write-Host '== Packages'
$Zip = Join-Path $WorkDir "Sqliteman-$Version-win64.zip"
if (Test-Path $Zip) { Remove-Item $Zip }
Compress-Archive -Path $Stage -DestinationPath $Zip

$iscc = Get-Command iscc -ErrorAction SilentlyContinue
if (-not $iscc) { $iscc = Get-Item "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe" -ErrorAction SilentlyContinue }
if ($iscc) {
    & $iscc "/DVersion=$Version" "/DStageDir=$Stage" "/DSourceDir=$SourceDir" "/O$WorkDir" `
        "$PSScriptRoot\sqliteman.iss"
    $Setup = Join-Path $WorkDir "Sqliteman-$Version-setup.exe"
    if ($SignCertificate) {
        signtool sign /n $SignCertificate /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 $Setup
    }
    Write-Host "== Done: $Zip, $Setup"
} else {
    Write-Host "== Done: $Zip (no installer: Inno Setup 6 was not found)"
}
