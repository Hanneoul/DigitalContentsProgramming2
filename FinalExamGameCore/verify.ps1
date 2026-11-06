$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$install = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
if (!$install) { throw 'Visual Studio with MSBuild is required.' }
$msbuild = Join-Path $install 'MSBuild\Current\Bin\MSBuild.exe'
foreach ($platform in @('x64', 'Win32')) {
    foreach ($configuration in @('Debug', 'Release')) {
        & $msbuild (Join-Path $projectRoot 'FinalExamGameCore.vcxproj') /nologo /verbosity:minimal /t:Build "/p:Configuration=$configuration" "/p:Platform=$platform"
        if ($LASTEXITCODE -ne 0) { throw "Build failed: $platform $configuration" }
        $exe = Join-Path $projectRoot "bin\$platform\$configuration\FinalExamGameCore.exe"
        & $exe --self-test
        if ($LASTEXITCODE -ne 0) { throw "Self-test failed: $platform $configuration" }
        $output = & $exe --fast --plain --max-rounds 100
        if ($LASTEXITCODE -ne 0 -or !($output -match '^WINNER: P[0-5]$')) { throw "Game failed: $platform $configuration" }
        $runDirectory = ($output | Where-Object { $_ -like 'Snapshots / diagnostics: *' }) -replace '^Snapshots / diagnostics: ', ''
        $badLogs = Get-ChildItem -LiteralPath $runDirectory -Filter '*_diagnostics.log' | Where-Object Length -gt 0
        if ($badLogs) { throw "Unexpected diagnostics: $($badLogs.FullName)" }
        Write-Host "PASS: $platform $configuration (self-test and complete game)"
    }
}

# Build a separate fault-injection copy; never change the submitted student files.
$probeRoot = Join-Path $projectRoot 'verification\probe'
New-Item -ItemType Directory -Force -Path $probeRoot | Out-Null
Get-ChildItem -LiteralPath $projectRoot -File | Where-Object { $_.Extension -in @('.c', '.h', '.vcxproj', '.dat') } |
    Copy-Item -Destination $probeRoot -Force
@'
#include "player_api.h"
#include <windows.h>
Command player0_choose_command(const char *path)
{
    const char *mode = getenv("FGC_TEST_PROBE");
    if (!mode) return COMMAND_UP;
    if (!strcmp(mode, "leak")) {
        void *p = malloc(41);
        FILE *f = fopen(path, "r");
        if (!p || !f) return (Command)99;
        memset(p, 0x5a, 41);
        return COMMAND_UP; /* Both intentional leaks must identify this file. */
    }
    if (!strcmp(mode, "timeout")) { Sleep(10000); return COMMAND_UP; }
    if (!strcmp(mode, "crash")) { RaiseException(0xE0000001, EXCEPTION_NONCONTINUABLE, 0, NULL); }
    if (!strcmp(mode, "invalid")) return (Command)999;
    if (!strcmp(mode, "double-close")) {
        FILE *f = fopen(path, "r");
        if (f) { fclose(f); fclose(f); }
    }
    return COMMAND_UP;
}
'@ | Set-Content -Encoding utf8 (Join-Path $probeRoot 'player0.c')
& $msbuild (Join-Path $probeRoot 'FinalExamGameCore.vcxproj') /nologo /verbosity:minimal /t:Build /p:Configuration=Debug /p:Platform=x64
if ($LASTEXITCODE -ne 0) { throw 'Fault-injection build failed.' }
$probeExe = Join-Path $probeRoot 'bin\x64\Debug\FinalExamGameCore.exe'
$oldProbe = $env:FGC_TEST_PROBE
try {
    foreach ($mode in @('leak', 'timeout', 'crash', 'invalid', 'double-close')) {
        $env:FGC_TEST_PROBE = $mode
        $output = & $probeExe --fast --plain --max-rounds 1
        if ($LASTEXITCODE -ne 0 -or !($output -match '^DRAW:')) { throw "Core did not survive: $mode" }
        $runDirectory = ($output | Where-Object { $_ -like 'Snapshots / diagnostics: *' }) -replace '^Snapshots / diagnostics: ', ''
        $log = Get-Content -Raw (Join-Path $runDirectory 'player0_diagnostics.log')
        $pattern = switch ($mode) {
            'leak' { 'Detected memory leaks!' }
            'timeout' { 'AI timeout' }
            'crash' { 'invalid command or worker crash' }
            'invalid' { 'invalid command or worker crash' }
            'double-close' { 'invalid/untracked fclose' }
        }
        if ($log -notmatch $pattern) { throw "Missing diagnostic for ${mode}: $log" }
        if ($mode -eq 'leak' -and ($log -notmatch 'player0.c\(\d+\)' -or $log -notmatch 'FILE LEAK')) {
            throw "Missing source location or FILE leak: $log"
        }
        Write-Host "PASS: fault injection $mode"
    }
} finally {
    $env:FGC_TEST_PROBE = $oldProbe
}
