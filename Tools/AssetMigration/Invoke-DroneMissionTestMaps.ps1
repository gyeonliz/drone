[CmdletBinding()]
param(
    [ValidateSet('Validate', 'Rebuild')]
    [string]$Mode = 'Validate',

    [switch]$MoveSmartObjectMap,

    [string]$ProjectPath,

    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$editorPath = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$builderPath = Join-Path $PSScriptRoot 'BuildDroneMissionSystemsTestMap.py'
$moverPath = Join-Path $PSScriptRoot 'MoveDroneTestMaps.py'
$projectRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent

if ([string]::IsNullOrWhiteSpace($ProjectPath)) {
    $ProjectPath = Join-Path $projectRoot 'Drone.uproject'
}

if (Get-Process UnrealEditor, UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    throw 'Close Unreal Editor before moving or building Drone test maps.'
}

foreach ($requiredPath in @($editorPath, $ProjectPath, $builderPath, $moverPath)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required file not found: $requiredPath"
    }
}

function Invoke-DroneEditorPython {
    param(
        [Parameter(Mandatory)]
        [string]$PythonPath,

        [Parameter(Mandatory)]
        [string]$RunName,

        [Parameter(Mandatory)]
        [string]$SuccessPattern,

        [Parameter(Mandatory)]
        [string]$FailurePattern
    )

    $runRoot = Join-Path $projectRoot ('Saved\Automation\MissionTestMapSetup\' + $RunName + '_' + [guid]::NewGuid().ToString('N'))
    $userDir = Join-Path $runRoot 'User'
    $logPath = Join-Path $runRoot ($RunName + '.log')
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
        ('-ExecutePythonScript=' + $PythonPath),
        ('-UserDir=' + $userDir),
        ('-abslog=' + $logPath)
    )

    & $editorPath @editorArgs
    $editorExitCode = $LASTEXITCODE
    if (-not (Test-Path -LiteralPath $logPath -PathType Leaf)) {
        throw "Unreal Editor did not create a log: $logPath"
    }

    $failure = Select-String -LiteralPath $logPath -Pattern $FailurePattern -Quiet
    $success = Select-String -LiteralPath $logPath -Pattern $SuccessPattern -Quiet
    Select-String -LiteralPath $logPath -Pattern 'DRONE_TESTMAP_MOVE\||DRONE_MISSION_TESTMAP\||MapCheck:' |
        ForEach-Object { $_.Line }

    if ($editorExitCode -ne 0 -or $failure -or -not $success) {
        throw "$RunName failed. Exit=$editorExitCode Log=$logPath"
    }
    Write-Output "$RunName succeeded. Log=$logPath"
}

if ($MoveSmartObjectMap) {
    $moveArguments = @{
        PythonPath = $moverPath
        RunName = 'MoveSmartObjectMap'
        SuccessPattern = 'DRONE_TESTMAP_MOVE\|VALIDATION_OK'
        FailurePattern = 'DRONE_TESTMAP_MOVE\|FAILED|LogPython: Error|Python script executed with errors'
    }
    Invoke-DroneEditorPython @moveArguments
}

try {
    if ($Mode -eq 'Rebuild') {
        $env:DRONE_MISSION_TESTMAP_REBUILD = '1'
    } else {
        Remove-Item Env:DRONE_MISSION_TESTMAP_REBUILD -ErrorAction SilentlyContinue
    }

    $buildArguments = @{
        PythonPath = $builderPath
        RunName = ('MissionSystemsTestMap_' + $Mode)
        SuccessPattern = 'DRONE_MISSION_TESTMAP\|VALIDATION_OK'
        FailurePattern = 'DRONE_MISSION_TESTMAP\|FAILED|LogPython: Error|Python script executed with errors'
    }
    Invoke-DroneEditorPython @buildArguments
} finally {
    Remove-Item Env:DRONE_MISSION_TESTMAP_REBUILD -ErrorAction SilentlyContinue
}
