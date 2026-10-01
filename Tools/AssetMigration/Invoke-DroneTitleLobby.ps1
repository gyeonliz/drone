[CmdletBinding()]
param(
    [string]$ProjectPath,
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [string]$TitleAssetDir
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if ([string]::IsNullOrWhiteSpace($ProjectPath)) { $ProjectPath = Join-Path $projectRoot 'Drone.uproject' }
$editorPath = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$scriptPath = Join-Path $PSScriptRoot 'ConfigureDroneTitleLobby.py'
foreach ($path in @($editorPath, $ProjectPath, $scriptPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing required file: $path" }
}
if (Get-Process UnrealEditor, UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    throw 'Close Unreal Editor before configuring Title/Lobby assets.'
}
$runRoot = Join-Path $projectRoot ('Saved\Automation\TitleLobbySetup\' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $runRoot -Force | Out-Null
$logPath = Join-Path $runRoot 'Setup.log'
$previousTitlePath = $env:DRONE_TITLE_ASSET_DIR
try {
    if ($TitleAssetDir) { $env:DRONE_TITLE_ASSET_DIR = $TitleAssetDir }
    & $editorPath $ProjectPath -unattended -nop4 -nullrhi -nosound -nosplash `
        -EnablePlugins=PythonScriptPlugin -ScriptErrorsAreFatal `
        ('-ExecutePythonScript=' + $scriptPath) ('-abslog=' + $logPath)
    $exitCode = $LASTEXITCODE
} finally {
    if ($null -eq $previousTitlePath) { Remove-Item Env:DRONE_TITLE_ASSET_DIR -ErrorAction SilentlyContinue }
    else { $env:DRONE_TITLE_ASSET_DIR = $previousTitlePath }
}
if (-not (Test-Path -LiteralPath $logPath)) { throw 'Unreal did not create a setup log.' }
$failed = Select-String -LiteralPath $logPath -Pattern 'DRONE_TITLE_LOBBY\|FAILED|LogPython: Error:' -Quiet
$passed = Select-String -LiteralPath $logPath -Pattern 'DRONE_TITLE_LOBBY\|VALIDATION_OK' -Quiet
Select-String -LiteralPath $logPath -Pattern 'DRONE_TITLE_LOBBY\||DRONE_TUTORIAL_MISSION_TEST\||MapCheck:' |
    ForEach-Object { $_.Line }
if ($exitCode -ne 0 -or $failed -or -not $passed) { throw "Title/Lobby setup failed. Log=$logPath" }
Write-Output "Title/Lobby setup succeeded. Log=$logPath"
