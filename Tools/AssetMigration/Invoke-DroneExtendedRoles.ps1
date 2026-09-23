[CmdletBinding()]
param(
    [string]$ProjectPath,
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$editorPath = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$builderPath = Join-Path $PSScriptRoot 'BuildDroneExtendedRoles.py'
$projectRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if ([string]::IsNullOrWhiteSpace($ProjectPath)) {
    $ProjectPath = Join-Path $projectRoot 'Drone.uproject'
}
if (Get-Process UnrealEditor, UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    throw 'Close Unreal Editor before building extended Drone roles.'
}
foreach ($requiredPath in @($editorPath, $ProjectPath, $builderPath)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required file not found: $requiredPath"
    }
}

$runRoot = Join-Path $projectRoot ('Saved\Automation\ExtendedRoleSetup\' + [guid]::NewGuid().ToString('N'))
$userDir = Join-Path $runRoot 'User'
$logPath = Join-Path $runRoot 'ExtendedRoles.log'
New-Item -ItemType Directory -Path $runRoot -Force | Out-Null

$editorArgs = @(
    $ProjectPath,
    '-unattended',
    '-nop4',
    '-nullrhi',
    '-nosound',
    '-nosplash',
    '-NoAssetRegistryCache',
    '-EnablePlugins=PythonScriptPlugin',
    '-ScriptErrorsAreFatal',
    ('-ExecutePythonScript=' + $builderPath),
    ('-UserDir=' + $userDir),
    ('-abslog=' + $logPath)
)
& $editorPath @editorArgs
$editorExitCode = $LASTEXITCODE
if (-not (Test-Path -LiteralPath $logPath -PathType Leaf)) {
    throw "Unreal Editor did not create a log: $logPath"
}
Select-String -LiteralPath $logPath -Pattern 'DRONE_EXTENDED_ROLES\|' | ForEach-Object { $_.Line }
$failure = Select-String -LiteralPath $logPath -Pattern 'DRONE_EXTENDED_ROLES\|FAILED|LogPython: Error|Python script executed with errors' -Quiet
$success = Select-String -LiteralPath $logPath -Pattern 'DRONE_EXTENDED_ROLES\|COMPLETE' -Quiet
if ($editorExitCode -ne 0 -or $failure -or -not $success) {
    throw "Extended Drone role setup failed. Exit=$editorExitCode Log=$logPath"
}
Write-Output "Extended Drone role setup succeeded. Log=$logPath"
