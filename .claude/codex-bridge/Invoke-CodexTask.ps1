<#
.SYNOPSIS
    Claude가 Codex에게 코드 외 작업(문서·Space·리뷰·조사)을 비대화형으로 맡긴다.

.DESCRIPTION
    1. roles/common.md + roles/<Role>.md + 작업 지시서(BriefFile)를 합쳐 runs/<시각-역할-이름>/prompt.md로 저장한다.
       common.md의 {{DRONE_REPO}}, {{MD_REPO}}는 이 PC의 실제 경로로 바뀐다.
    2. codex exec를 역할별 샌드박스로 실행한다.
         docs     : md 문서 저장소만 쓰기 가능(workspace-write). Unreal 저장소는 읽기만 가능.
                    Drone Space 저장 같은 도구 승인은 --approve-for-me(자동 검토)로 처리한다.
         review   : Unreal 저장소 기준 읽기 전용.
         research : Unreal 저장소 기준 읽기 전용.
       → 어떤 역할이든 Codex는 Unreal 저장소의 코드·자산을 고칠 수 없다.
    3. Codex의 마지막 답변을 runs/.../result.md, 실행 로그를 stdout.log / stderr.log, 요약을 meta.json에 남긴다.

    한글 프롬프트를 PowerShell 5.1 인자로 넘기면 깨질 수 있어서, Codex에는 "prompt.md를 읽어라"는 영문 한 줄만 넘긴다.
    저장소 경로는 BridgeCommon.ps1이 PC마다 찾는다(다른 PC 설정은 md 저장소 docs/git/CLAUDE_CODEX_SETUP.md).

.EXAMPLE
    .\Invoke-CodexTask.ps1 -Role docs -BriefFile .\briefs\lobby-fix.md -Name lobby-fix
    .\Invoke-CodexTask.ps1 -Role review -BriefFile .\briefs\review.md -Name review -Effort high
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('docs', 'review', 'research')][string]$Role,
    [Parameter(Mandatory)][string]$BriefFile,
    # 실행 폴더 이름에 붙는다. 영문·숫자·하이픈만 쓴다.
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9-]{1,40}$')][string]$Name,
    # Codex 기본 설정은 xhigh라 사용량이 크다. 문서 갱신은 medium이면 충분하다.
    [ValidateSet('low', 'medium', 'high', 'xhigh')][string]$Effort = 'medium',
    # docs 역할에서 Trello·웹 읽기가 필요할 때만 켠다. Space 도구 호출에는 필요 없다.
    [switch]$Network,
    # md 저장소 위치를 직접 지정할 때만 쓴다. 보통은 자동으로 찾는다.
    [string]$MdRepo
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'BridgeCommon.ps1')

$DroneRepo = Get-DroneRepoRoot
$MdRepoPath = Get-MdRepoRoot $MdRepo
if (-not $MdRepoPath) { throw 'md 문서 저장소를 찾지 못했다. Test-CollabSetup.ps1 -MdRepo <경로> -WriteLocalConfig 로 이 PC 경로를 등록한다.' }
$Codex = Find-CodexExe
if (-not $Codex) { throw 'codex.exe를 찾지 못했다. Codex 데스크톱 앱 설치·로그인을 확인한다.' }

$BriefPath = (Resolve-Path -LiteralPath $BriefFile).Path
$Stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$RunDir = Join-Path $PSScriptRoot "runs\$Stamp-$Role-$Name"
New-Item -ItemType Directory -Force -Path $RunDir | Out-Null

$Utf8 = New-Object System.Text.UTF8Encoding($false)
$Prompt = (@(
    [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'roles\common.md'), $Utf8),
    [IO.File]::ReadAllText((Join-Path $PSScriptRoot "roles\$Role.md"), $Utf8),
    "# 작업 지시서 (Claude 작성)`n",
    [IO.File]::ReadAllText($BriefPath, $Utf8)
) -join "`n`n---`n`n").Replace('{{DRONE_REPO}}', $DroneRepo).Replace('{{MD_REPO}}', $MdRepoPath)
$PromptPath = Join-Path $RunDir 'prompt.md'
[IO.File]::WriteAllText($PromptPath, $Prompt, $Utf8)

$ResultPath = Join-Path $RunDir 'result.md'
$Instruction = "Read the task file at $PromptPath as UTF-8 and follow it exactly. Reply in Korean using the report format defined in that file."

if ($Role -eq 'docs') {
    # --approve-for-me는 workspace-write 샌드박스를 스스로 쓰며 -s와 함께 줄 수 없다(codex-cli 0.159.2).
    $Sandbox = @('--approve-for-me', '-C', "`"$MdRepoPath`"")
    if ($Network) { $Sandbox += @('-c', 'sandbox_workspace_write.network_access=true') }
} else {
    $Sandbox = @('-s', 'read-only', '-C', "`"$DroneRepo`"")
}

$ArgList = @('exec') + $Sandbox + @('-c', "model_reasoning_effort=$Effort", '-o', "`"$ResultPath`"", "`"$Instruction`"")
$Started = Get-Date
$Proc = Start-Process -FilePath $Codex -ArgumentList $ArgList -NoNewWindow -Wait -PassThru `
    -RedirectStandardOutput (Join-Path $RunDir 'stdout.log') -RedirectStandardError (Join-Path $RunDir 'stderr.log')

$Meta = [ordered]@{
    role = $Role; name = $Name; effort = $Effort; network = [bool]$Network
    computer = $env:COMPUTERNAME; droneRepo = $DroneRepo; mdRepo = $MdRepoPath
    codex = $Codex; exitCode = $Proc.ExitCode
    started = $Started.ToString('s'); finished = (Get-Date).ToString('s')
    brief = $BriefPath; result = $ResultPath
}
[IO.File]::WriteAllText((Join-Path $RunDir 'meta.json'), ($Meta | ConvertTo-Json), $Utf8)

Write-Output "CODEX_EXIT=$($Proc.ExitCode)"
Write-Output "RUN_DIR=$RunDir"
if (Test-Path -LiteralPath $ResultPath) {
    Write-Output '----- result.md -----'
    Write-Output ([IO.File]::ReadAllText($ResultPath, $Utf8))
} else {
    Write-Output 'result.md가 없다. stderr.log를 확인한다.'
}
exit $Proc.ExitCode
