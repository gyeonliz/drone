[CmdletBinding()]
param(
    [ValidateSet('Validate', 'Rebuild')]
    [string]$Mode = 'Validate',
    [string]$ProjectPath,
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$editorPath = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$pythonPath = Join-Path $PSScriptRoot 'BuildDroneTutorialMissionTest.py'
$projectRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if ([string]::IsNullOrWhiteSpace($ProjectPath)) {
    $ProjectPath = Join-Path $projectRoot 'Drone.uproject'
}
foreach ($requiredPath in @($editorPath, $ProjectPath, $pythonPath)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required file not found: $requiredPath"
    }
}
if (Get-Process UnrealEditor, UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    throw 'Close Unreal Editor before building the Tutorial Mission test map.'
}

$runRoot = Join-Path $projectRoot ('Saved\Automation\TutorialMissionTestSetup\' + [guid]::NewGuid().ToString('N'))
$userDir = Join-Path $runRoot 'User'
$logPath = Join-Path $runRoot 'Setup.log'
New-Item -ItemType Directory -Path $runRoot -Force | Out-Null

try {
    if ($Mode -eq 'Rebuild') {
        $env:DRONE_TUTORIAL_MISSION_REBUILD = '1'
    } else {
        Remove-Item Env:DRONE_TUTORIAL_MISSION_REBUILD -ErrorAction SilentlyContinue
    }
    & $editorPath @(
        $ProjectPath,
        '-unattended',
        '-nop4',
        '-nullrhi',
        '-nosound',
        '-nosplash',
        '-NoAssetRegistryCache',
        '-EnablePlugins=PythonScriptPlugin',
        '-ScriptErrorsAreFatal',
        ('-ExecutePythonScript=' + $pythonPath),
        ('-UserDir=' + $userDir),
        ('-abslog=' + $logPath)
    )
    $exitCode = $LASTEXITCODE
} finally {
    Remove-Item Env:DRONE_TUTORIAL_MISSION_REBUILD -ErrorAction SilentlyContinue
}

if (-not (Test-Path -LiteralPath $logPath -PathType Leaf)) {
    throw "Unreal Editor did not create a log: $logPath"
}
$failed = Select-String -LiteralPath $logPath -Pattern 'DRONE_TUTORIAL_MISSION_TEST\|FAILED|LogPython: Error:|Python script executed with errors' -Quiet
$succeeded = Select-String -LiteralPath $logPath -Pattern 'DRONE_TUTORIAL_MISSION_TEST\|VALIDATION_OK' -Quiet
# Keep this parser ASCII-only so Windows PowerShell 5.1 can execute a UTF-8
# script without a BOM. Build the localized words from Unicode code points.
$localizedErrorWord = [string]([char]0xC624) + [string]([char]0xB958)
$localizedWarningWord = [string]([char]0xACBD) + [string]([char]0xACE0)
$mapCheckLines = @(Select-String -LiteralPath $logPath -Pattern 'MapCheck:')
$mapCheckSummaries = @($mapCheckLines | Where-Object {
    ($_.Line -match [regex]::Escape($localizedErrorWord) -and $_.Line -match [regex]::Escape($localizedWarningWord)) -or
    $_.Line -match '(?i)errors?.*warnings?'
})
$lastMapCheck = if ($mapCheckSummaries.Count -gt 0) { $mapCheckSummaries[-1].Line } else { '' }
$localizedZeroPattern = [regex]::Escape($localizedErrorWord) + '\s*0\s*.*' + [regex]::Escape($localizedWarningWord) + '\s*0'
$mapCheckOk = ($lastMapCheck -match $localizedZeroPattern) -or ($lastMapCheck -match '(?i)0 errors?.*0 warnings?')
Select-String -LiteralPath $logPath -Pattern 'DRONE_TUTORIAL_MISSION_TEST\||MapCheck:' |
    ForEach-Object { $_.Line }
if ($exitCode -ne 0 -or $failed -or -not $succeeded -or -not $mapCheckOk) {
    throw "Tutorial Mission Test $Mode failed. Exit=$exitCode Log=$logPath"
}
Write-Output "Tutorial Mission Test $Mode succeeded. Log=$logPath"
