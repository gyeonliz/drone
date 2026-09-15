[CmdletBinding()]
param(
    [ValidateSet('Validate', 'Rebuild')]
    [string]$Mode = 'Validate',

    [string]$ProjectPath,

    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$editorPath = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$pythonPath = Join-Path $PSScriptRoot 'BuildDroneTutorialSystemsTestMap.py'
$projectRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent

if ([string]::IsNullOrWhiteSpace($ProjectPath)) {
    $ProjectPath = Join-Path $projectRoot 'Drone.uproject'
}

$runRoot = Join-Path $projectRoot ('Saved\Automation\TutorialSystemsTestMapSetup\' + [guid]::NewGuid().ToString('N'))
$userDir = Join-Path $runRoot 'User'
$logPath = Join-Path $runRoot 'Setup.log'

if (-not (Test-Path -LiteralPath $editorPath -PathType Leaf)) {
    throw "UnrealEditor-Cmd.exe not found: $editorPath"
}
if (-not (Test-Path -LiteralPath $ProjectPath -PathType Leaf)) {
    throw "Project not found: $ProjectPath"
}
if (-not (Test-Path -LiteralPath $pythonPath -PathType Leaf)) {
    throw "Setup script not found: $pythonPath"
}
if (Get-Process UnrealEditor, UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    throw 'Close Unreal Editor before rebuilding or validating the Tutorial Systems Test Map.'
}

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
    ('-ExecutePythonScript=' + $pythonPath),
    ('-UserDir=' + $userDir),
    ('-abslog=' + $logPath)
)

try {
    if ($Mode -eq 'Rebuild') {
        $env:DRONE_TUTORIAL_TESTMAP_REBUILD = '1'
    } else {
        Remove-Item Env:DRONE_TUTORIAL_TESTMAP_REBUILD -ErrorAction SilentlyContinue
    }

    & $editorPath @editorArgs
    $editorExitCode = $LASTEXITCODE
} finally {
    Remove-Item Env:DRONE_TUTORIAL_TESTMAP_REBUILD -ErrorAction SilentlyContinue
}

if (-not (Test-Path -LiteralPath $logPath -PathType Leaf)) {
    throw "Unreal Editor did not create a log: $logPath"
}

$failure = Select-String -LiteralPath $logPath -Pattern 'DRONE_TUTORIAL_TESTMAP\|FAILED|LogPython: Error|Python script executed with errors' -Quiet
$validationOk = Select-String -LiteralPath $logPath -Pattern 'DRONE_TUTORIAL_TESTMAP\|VALIDATION_OK' -Quiet
$mapCheckOk = Select-String -LiteralPath $logPath -Pattern 'MapCheck:.*(오류 0 회.*경고 0 회|0 errors?.*0 warnings?)' -Quiet

Select-String -LiteralPath $logPath -Pattern 'DRONE_TUTORIAL_TESTMAP\||MapCheck:' |
    ForEach-Object { $_.Line }

if ($editorExitCode -ne 0 -or $failure -or -not $validationOk -or -not $mapCheckOk) {
    throw "Tutorial Systems Test Map $Mode failed. Exit=$editorExitCode Log=$logPath"
}

Write-Output "Tutorial Systems Test Map $Mode succeeded. Log=$logPath"
