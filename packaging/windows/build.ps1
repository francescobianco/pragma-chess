# Builds the Windows installer of Pragma Chess.
#
#   packaging\windows\build.ps1 [-Version x.y.z]
#
# Run it from a Visual Studio developer shell (MSVC on PATH) with Qt 6 for
# MSVC 64-bit on PATH (qmake/windeployqt), CMake, Ninja and Inno Setup 6.
# The installer lands in dist\.
param(
    [string]$Version = ""
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path "$PSScriptRoot\..\..").Path
$Build = "$Root\build-windows"
$Stage = "$Build\stage\Pragma Chess"
$Dist = "$Root\dist"

function Invoke-Checked {
    param([string]$Command, [string[]]$Arguments)
    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Command failed with exit code $LASTEXITCODE" }
}

$configure = @("-S", $Root, "-B", $Build, "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release",
    # Always the bundled yaml-cpp, linked statically: nothing to ship next to the exe.
    "-DCMAKE_DISABLE_FIND_PACKAGE_yaml-cpp=ON")
if ($Version) { $configure += "-DPRAGMA_VERSION=$Version" }
Invoke-Checked cmake $configure
Invoke-Checked cmake @("--build", $Build)
if ($env:SKIP_TESTS -ne "1") {
    $env:QT_QPA_PLATFORM = "offscreen"
    Invoke-Checked ctest @("--test-dir", $Build, "--output-on-failure")
    Remove-Item Env:QT_QPA_PLATFORM
}

# The application folder: the exe, the Qt it needs and the C++ runtime.
if (Test-Path "$Build\stage") { Remove-Item -Recurse -Force "$Build\stage" }
New-Item -ItemType Directory -Force -Path $Stage | Out-Null
Copy-Item "$Build\gui\qt\pragma-chess.exe" $Stage
Copy-Item "$Build\gui\qt\pragma-explain.exe" $Stage
Copy-Item "$Build\gui\qt\pragma-book.exe" $Stage
Copy-Item "$Root\LICENSE" "$Stage\LICENSE.txt"
Invoke-Checked windeployqt @("--release", "--no-compiler-runtime", "--no-opengl-sw",
    "--no-system-d3d-compiler", "--no-quick-import",
    "$Stage\pragma-chess.exe")

# Only SQLite is used: the other drivers need client libraries we do not ship.
Get-ChildItem "$Stage\sqldrivers" -Filter *.dll |
    Where-Object { $_.Name -notlike "qsqlite*" } | Remove-Item

# The Visual C++ runtime, next to the exe (no separate redistributable to run).
$crt = Get-ChildItem "$env:VCToolsRedistDir\x64" -Directory -Filter "Microsoft.VC*.CRT" | Select-Object -First 1
if (-not $crt) { throw "Visual C++ runtime not found under VCToolsRedistDir ($env:VCToolsRedistDir)" }
Copy-Item "$($crt.FullName)\*.dll" $Stage

# The installer.
$iscc = (Get-Command iscc -ErrorAction SilentlyContinue).Source
if (-not $iscc) { $iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe" }
if (-not (Test-Path $iscc)) { throw "Inno Setup 6 (ISCC.exe) not found" }
$appVersion = if ($Version) { $Version } else {
    (Select-String -Path "$Build\CMakeCache.txt" -Pattern "^PRAGMA_VERSION:STRING=(.*)$").Matches[0].Groups[1].Value
}
New-Item -ItemType Directory -Force -Path $Dist | Out-Null
Invoke-Checked $iscc @("/Q", "/DAppVersion=$appVersion", "/DSourceDir=$Stage", "/DOutputDir=$Dist",
    "$PSScriptRoot\pragma-chess.iss")

# A portable zip for those who do not want to install.
$zip = "$Dist\PragmaChess-$appVersion-windows-x64-portable.zip"
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path $Stage -DestinationPath $zip
Get-ChildItem $Dist
