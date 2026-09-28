[CmdletBinding()]
param(
    [string]$ProjectPath,
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$editorPath = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$frameworkBuilder = Join-Path $PSScriptRoot 'BuildDroneMissionFramework.py'
$animationBuilder = Join-Path $PSScriptRoot 'BuildNPCGreyboxAnimationAssets.py'
$animationVerifier = Join-Path $PSScriptRoot 'VerifyNPCGreyboxAnimationAssets.py'
$projectRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent

if ([string]::IsNullOrWhiteSpace($ProjectPath)) {
    $ProjectPath = Join-Path $projectRoot 'Drone.uproject'
}
if (Get-Process UnrealEditor, UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
    throw 'Close Unreal Editor before building the Mission Framework assets.'
}
foreach ($requiredPath in @($editorPath, $ProjectPath, $frameworkBuilder, $animationBuilder, $animationVerifier)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required file not found: $requiredPath"
    }
}

function Invoke-DroneEditorPython {
    param(
        [Parameter(Mandatory)][string]$PythonPath,
        [Parameter(Mandatory)][string]$RunName,
        [Parameter(Mandatory)][string]$SuccessPattern
    )
    $runRoot = Join-Path $projectRoot ('Saved\Automation\MissionFrameworkSetup\' + $RunName + '_' + [guid]::NewGuid().ToString('N'))
    $userDir = Join-Path $runRoot 'User'
    $logPath = Join-Path $runRoot ($RunName + '.log')
    New-Item -ItemType Directory -Path $runRoot -Force | Out-Null
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
        ('-ExecutePythonScript=' + $PythonPath),
        ('-UserDir=' + $userDir),
        ('-abslog=' + $logPath)
    )
    $exitCode = $LASTEXITCODE
    if (-not (Test-Path -LiteralPath $logPath -PathType Leaf)) {
        throw "Unreal Editor did not create a log: $logPath"
    }
    Select-String -LiteralPath $logPath -Pattern 'DRONE_MISSION_FRAMEWORK\||NPC_GREYBOX_ANIM' |
        ForEach-Object { $_.Line }
    $failed = Select-String -LiteralPath $logPath -Pattern 'DRONE_MISSION_FRAMEWORK\|FAILED|NPC_GREYBOX_ANIM.*FAILED|LogPython: Error:|Python script executed with errors' -Quiet
    $succeeded = Select-String -LiteralPath $logPath -Pattern $SuccessPattern -Quiet
    if ($exitCode -ne 0 -or $failed -or -not $succeeded) {
        throw "$RunName failed. Exit=$exitCode Log=$logPath"
    }
    Write-Output "$RunName succeeded. Log=$logPath"
}

Invoke-DroneEditorPython -PythonPath $frameworkBuilder -RunName 'MissionFramework' -SuccessPattern 'DRONE_MISSION_FRAMEWORK\|VALIDATION_OK'
Invoke-DroneEditorPython -PythonPath $animationBuilder -RunName 'NPCWalkingAnimation' -SuccessPattern 'NPC_GREYBOX_ANIM success'
Invoke-DroneEditorPython -PythonPath $animationVerifier -RunName 'NPCWalkingAnimationVerify' -SuccessPattern 'NPC_GREYBOX_ANIM_VERIFY success'
