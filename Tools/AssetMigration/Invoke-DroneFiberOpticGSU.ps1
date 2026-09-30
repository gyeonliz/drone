[CmdletBinding()]
param(
    [string]$ProjectPath,
    [string]$SourceRoot = 'C:\Users\Metacon_41\Downloads\새 폴더\광섬유',
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$editorPath = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$builderPath = Join-Path $PSScriptRoot 'ImportDroneFiberOpticGSU.py'
$projectRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if ([string]::IsNullOrWhiteSpace($ProjectPath)) {
    $ProjectPath = Join-Path $projectRoot 'Drone.uproject'
}
if (Get-Process UnrealEditor, UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    throw 'Close Unreal Editor before importing the Fiber Optic GSU Drone.'
}
foreach ($requiredPath in @($editorPath, $ProjectPath, $builderPath, (Join-Path $SourceRoot 'GSU.fbx'))) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required file not found: $requiredPath"
    }
}

$runRoot = Join-Path $projectRoot ('Saved\Automation\FiberOpticGSU\' + [guid]::NewGuid().ToString('N'))
$userDir = Join-Path $runRoot 'User'
$logPath = Join-Path $runRoot 'FiberOpticGSU.log'
New-Item -ItemType Directory -Path $runRoot -Force | Out-Null

$env:DRONE_FIBER_GSU_SOURCE = $SourceRoot
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
try {
    & $editorPath @editorArgs
    $editorExitCode = $LASTEXITCODE
}
finally {
    Remove-Item Env:DRONE_FIBER_GSU_SOURCE -ErrorAction SilentlyContinue
}
if (-not (Test-Path -LiteralPath $logPath -PathType Leaf)) {
    throw "Unreal Editor did not create a log: $logPath"
}
Select-String -LiteralPath $logPath -Pattern 'DRONE_FIBER_GSU\|' | ForEach-Object { $_.Line }
$failure = Select-String -LiteralPath $logPath -Pattern 'DRONE_FIBER_GSU\|FAILED|LogPython: Error|Python script executed with errors' -Quiet
$success = Select-String -LiteralPath $logPath -Pattern 'DRONE_FIBER_GSU\|COMPLETE' -Quiet
if ($editorExitCode -ne 0 -or $failure -or -not $success) {
    throw "Fiber Optic GSU import failed. Exit=$editorExitCode Log=$logPath"
}
Write-Output "Fiber Optic GSU import succeeded. Log=$logPath"
