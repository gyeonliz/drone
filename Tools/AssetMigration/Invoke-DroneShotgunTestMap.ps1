[CmdletBinding()]
param(
    [ValidateSet('Validate', 'Rebuild')]
    [string]$Mode = 'Validate',

    [string]$ProjectPath,

    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$editorPath = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$builderPath = Join-Path $PSScriptRoot 'BuildDroneShotgunSystemsTestMap.py'
$projectRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent

if ([string]::IsNullOrWhiteSpace($ProjectPath)) {
    $ProjectPath = Join-Path $projectRoot 'Drone.uproject'
}

if (Get-Process UnrealEditor, UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    throw 'Close Unreal Editor before building the Drone Shotgun test map.'
}

foreach ($requiredPath in @($editorPath, $ProjectPath, $builderPath)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required file not found: $requiredPath"
    }
}

$runRoot = Join-Path $projectRoot ('Saved\Automation\ShotgunTestMapSetup\' + $Mode + '_' + [guid]::NewGuid().ToString('N'))
$userDir = Join-Path $runRoot 'User'
$logPath = Join-Path $runRoot 'ShotgunTestMap.log'
New-Item -ItemType Directory -Path $runRoot -Force | Out-Null

try {
    if ($Mode -eq 'Rebuild') {
        $env:DRONE_SHOTGUN_TESTMAP_REBUILD = '1'
    } else {
        Remove-Item Env:DRONE_SHOTGUN_TESTMAP_REBUILD -ErrorAction SilentlyContinue
    }

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

    Select-String -LiteralPath $logPath -Pattern 'DRONE_SHOTGUN_TESTMAP\||MapCheck:' |
        ForEach-Object { $_.Line }
    $failure = Select-String -LiteralPath $logPath -Pattern 'DRONE_SHOTGUN_TESTMAP\|FAILED|LogPython: Error|Python script executed with errors' -Quiet
    $success = Select-String -LiteralPath $logPath -Pattern 'DRONE_SHOTGUN_TESTMAP\|VALIDATION_OK' -Quiet
    if ($editorExitCode -ne 0 -or $failure -or -not $success) {
        throw "Shotgun test map setup failed. Exit=$editorExitCode Log=$logPath"
    }
    Write-Output "Shotgun test map $Mode succeeded. Log=$logPath"
} finally {
    Remove-Item Env:DRONE_SHOTGUN_TESTMAP_REBUILD -ErrorAction SilentlyContinue
}
