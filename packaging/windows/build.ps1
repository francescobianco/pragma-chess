# Builds the Windows installer of Pragma Chess.
#
#   packaging\windows\build.ps1 [-Version x.y.z] [-Engine <dir>]
#
# Run it from a Visual Studio developer shell (MSVC on PATH) with Qt 6 for
# MSVC 64-bit on PATH (qmake/windeployqt), CMake, Ninja and Inno Setup 6.
# -Engine is a folder where scripts/build-stockfish.sh staged the engine for
# windows-x86-64 (CI builds it on Linux with MinGW); without it the script
# builds it here, which needs make and MinGW's g++ in Git's bash (MSYS2).
# Before the installer is made, every DLL the programs need is checked to be
# in the folder and the application is started from it with a bare PATH, so a
# package that cannot start on a clean Windows is never published.
# The installer lands in dist\.
param(
    [string]$Version = "",
    [string]$Engine = ""
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
    $log = "$Build\tests.txt"
    & "$Build\gui\qt\tst_chessrules.exe" -o "$log,txt"
    $code = $LASTEXITCODE
    if (Test-Path $log) { Get-Content $log | Out-Host }
    if ($code -ne 0) { throw "tests failed with exit code $code" }
    Remove-Item Env:QT_QPA_PLATFORM
}

# The application folder: the exe, the Qt it needs, the engine and the DLLs.
if (Test-Path "$Build\stage") { Remove-Item -Recurse -Force "$Build\stage" }
New-Item -ItemType Directory -Force -Path $Stage | Out-Null
Copy-Item "$Build\gui\qt\pragma-chess.exe" $Stage
Copy-Item "$Build\gui\qt\pragma-explain.exe" $Stage
Copy-Item "$Build\gui\qt\pragma-book.exe" $Stage
Copy-Item "$Root\LICENSE" "$Stage\LICENSE.txt"
# The bundled engine in engines\ (EngineCatalog), with its license and README.
if ($Engine) {
    New-Item -ItemType Directory -Force -Path "$Stage\engines" | Out-Null
    Copy-Item "$Engine\*" "$Stage\engines"
} else {
    # The script runs in Git for Windows' bash (the bash.exe on PATH may be WSL's).
    $bash = "$env:ProgramFiles\Git\bin\bash.exe"
    if (-not (Test-Path $bash)) { $bash = "bash" }
    $script = ($Root -replace '\\', '/') + "/scripts/build-stockfish.sh"
    $engines = ($Stage -replace '\\', '/') + "/engines"
    Invoke-Checked $bash @($script, $engines, "windows-x86-64")
}
# Qt's own translations are embedded in the application (UiLanguage), and the
# DirectX shader compiler is for Qt Quick, which we do not use.
Invoke-Checked windeployqt @("--release", "--no-compiler-runtime", "--no-opengl-sw",
    "--no-system-d3d-compiler", "--no-system-dxc-compiler", "--no-quick-import",
    "--no-translations", "$Stage\pragma-chess.exe")
Remove-Item -ErrorAction SilentlyContinue "$Stage\dxcompiler.dll", "$Stage\dxil.dll"

# Only SQLite is used: the other drivers need client libraries we do not ship.
Get-ChildItem "$Stage\sqldrivers" -Filter *.dll |
    Where-Object { $_.Name -notlike "qsqlite*" } | Remove-Item

# Every DLL the programs and plugins import must be in the folder or be part of
# Windows. windeployqt only knows Qt's: the Visual C++ runtime and OpenSSL
# (Phone Link's libdatachannel links it) are found here and copied, and the
# build fails on anything that cannot be found. The C++ runtime is always
# shipped, even if this machine has it in System32, as a user may not.
$crt = Get-ChildItem "$env:VCToolsRedistDir\x64" -Directory -Filter "Microsoft.VC*.CRT" | Select-Object -First 1
if (-not $crt) { throw "Visual C++ runtime not found under VCToolsRedistDir ($env:VCToolsRedistDir)" }
$searchDirs = @($crt.FullName)
$openssl = Select-String -Path "$Build\CMakeCache.txt" -Pattern "^OPENSSL_INCLUDE_DIR:PATH=(.*)$"
if ($openssl) {
    $opensslRoot = Split-Path -Parent $openssl.Matches[0].Groups[1].Value
    $searchDirs += @("$opensslRoot\bin", $opensslRoot)
}
$searchDirs += @($env:PATH -split ';' | Where-Object { $_ -and (Test-Path $_) })
$system = "$env:SystemRoot\System32"
function Test-Shipped([string]$Name) {
    return $Name -match '^(vcruntime|msvcp|concrt|vcomp)\d' -or $Name -match '^lib(ssl|crypto)-'
}
$missing = @()
do {
    $copied = 0
    foreach ($binary in Get-ChildItem $Stage -Recurse -Include *.exe, *.dll) {
        $imports = & dumpbin /nologo /dependents $binary.FullName |
            Where-Object { $_ -match '^\s+(\S+\.dll)\s*$' } | ForEach-Object { $Matches[1] }
        foreach ($dll in $imports) {
            if ($dll -match '^(api|ext)-ms-') { continue }
            if (Test-Path "$Stage\$dll") { continue }
            if (-not (Test-Shipped $dll) -and (Test-Path "$system\$dll")) { continue }
            $found = $searchDirs | ForEach-Object { "$_\$dll" } | Where-Object { Test-Path $_ } |
                Select-Object -First 1
            if ($found) {
                Write-Host "Shipping $dll (needed by $($binary.Name)) from $found"
                Copy-Item $found $Stage
                $copied++
            } elseif ($missing -notcontains "$dll ($($binary.Name))") {
                $missing += "$dll ($($binary.Name))"
            }
        }
    }
} while ($copied -gt 0)
if ($missing) { throw "DLLs needed but not found: $($missing -join ', ')" }

# The real test: start the programs from the folder with nothing on PATH but
# Windows, and with the "missing DLL" dialog turned into an exit code.
Add-Type -Namespace Native -Name Kernel32 -MemberDefinition `
    '[DllImport("kernel32.dll")] public static extern uint SetErrorMode(uint mode);'
[Native.Kernel32]::SetErrorMode(0x0003) | Out-Null
function Start-Staged([string]$File, [string]$Arguments, [switch]$Pipe) {
    $info = New-Object System.Diagnostics.ProcessStartInfo $File, $Arguments
    $info.UseShellExecute = $false
    $info.RedirectStandardInput = [bool]$Pipe
    $info.RedirectStandardOutput = [bool]$Pipe
    $info.WorkingDirectory = $env:TEMP
    $info.EnvironmentVariables["PATH"] = "$system;$env:SystemRoot"
    $info.EnvironmentVariables["PRAGMA_CHESS_DIR"] = "$Build\smoke-test"
    return [System.Diagnostics.Process]::Start($info)
}
$engineProcess = Start-Staged "$Stage\engines\stockfish.exe" "" -Pipe
$engineProcess.StandardInput.WriteLine("uci")
$engineProcess.StandardInput.WriteLine("isready")
$engineProcess.StandardInput.WriteLine("go depth 10")
Start-Sleep -Seconds 5
$engineProcess.StandardInput.WriteLine("quit")
if (-not $engineProcess.WaitForExit(10000)) { $engineProcess.Kill(); throw "the engine does not quit" }
$engineOutput = $engineProcess.StandardOutput.ReadToEnd()
if ($engineOutput -notmatch 'uciok' -or $engineOutput -notmatch 'bestmove') {
    throw "the engine does not answer (exit code $($engineProcess.ExitCode)):`n$engineOutput"
}
Write-Host "Engine: $(($engineOutput -split "`n" | Select-String '^bestmove') -join '')"
$app = Start-Staged "$Stage\pragma-chess.exe" ""
if ($app.WaitForExit(15000)) {
    throw ("pragma-chess.exe does not start from the package: exit code 0x{0:X8}" -f $app.ExitCode)
}
$app.Kill()
Write-Host "pragma-chess.exe starts from the package"

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
