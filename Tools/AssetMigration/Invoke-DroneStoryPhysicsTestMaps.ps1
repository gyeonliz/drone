param(
    [switch]$ValidateOnly,
    [switch]$Rebuild
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$ProjectFile = Join-Path $ProjectRoot 'Drone.uproject'
$ScriptFile = Join-Path $PSScriptRoot 'BuildDroneStoryPhysicsTestMaps.py'
$EditorCmd = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$LogFile = Join-Path $ProjectRoot 'Saved\Logs\DroneStoryPhysicsTestMaps.log'

if (-not (Test-Path -LiteralPath $EditorCmd)) { throw "UnrealEditor-Cmd not found: $EditorCmd" }
if (-not (Test-Path -LiteralPath $ProjectFile)) { throw "Project not found: $ProjectFile" }
if (-not (Test-Path -LiteralPath $ScriptFile)) { throw "Python tool not found: $ScriptFile" }

$env:DRONE_STORY_PHYSICS_VALIDATE_ONLY = if ($ValidateOnly) { '1' } else { '0' }
$env:DRONE_STORY_PHYSICS_REBUILD = if ($Rebuild) { '1' } else { '0' }

& $EditorCmd $ProjectFile '-run=pythonscript' "-script=$ScriptFile" '-unattended' '-nop4' '-nosplash' '-stdout' '-FullStdOutLogOutput' "-abslog=$LogFile"
$ExitCode = $LASTEXITCODE
Remove-Item Env:DRONE_STORY_PHYSICS_VALIDATE_ONLY -ErrorAction SilentlyContinue
Remove-Item Env:DRONE_STORY_PHYSICS_REBUILD -ErrorAction SilentlyContinue
if ($ExitCode -ne 0) { throw "Story/Physics map tool failed with exit code $ExitCode. See $LogFile" }

$Success = Select-String -LiteralPath $LogFile -Pattern 'DRONE_STORY_PHYSICS_TEST\|VALIDATION_OK\|physics=1\|story_maps=4\|story_missions=4' -Quiet
if (-not $Success) { throw "Validation marker missing. See $LogFile" }
Write-Host "Story/Physics test maps validated. Log: $LogFile"
