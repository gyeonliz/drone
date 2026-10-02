<#
.SYNOPSIS
    이 PC에서 Claude ↔ Codex 협업이 바로 동작하는지 점검하고, 필요하면 PC 전용 설정을 만든다.

.DESCRIPTION
    Git으로 따라오는 것(공유): CLAUDE.md, AGENTS.md, .mcp.json, .claude/settings.json, .claude/skills, .claude/codex-bridge(스크립트·역할)
    PC마다 따로 두는 것(Git 제외): .claude/settings.local.json, .claude/codex-bridge/local.json, runs/, briefs/,
                                  사용자 홈의 ~/.claude(로그인·메모리·사용자 설정), ~/.codex(로그인·세션)

    -WriteLocalConfig 를 주면 이 PC의 md 저장소 경로를
      .claude/codex-bridge/local.json          (Codex 브리지가 읽음)
      .claude/settings.local.json              (Claude가 md 저장소를 추가 작업 폴더로 인식)
    에 기록한다. 기존 파일이 있으면 해당 값만 바꾼다.

.EXAMPLE
    .\Test-CollabSetup.ps1
    .\Test-CollabSetup.ps1 -MdRepo 'D:\JGY\project\md' -WriteLocalConfig
#>
[CmdletBinding()]
param(
    [string]$MdRepo,
    [switch]$WriteLocalConfig,
    # 사용자 공통 규칙(md docs/git/USER_RULES.md)을 이 PC의 ~/.claude/CLAUDE.md에 가져오기 줄로 설치한다. 기존 내용은 지우지 않는다.
    [switch]$InstallUserRules
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'BridgeCommon.ps1')
$Utf8 = New-Object System.Text.UTF8Encoding($false)
$Problems = 0

function Write-Check([string]$State, [string]$Message) {
    # OK / WARN / FAIL 세 단계. FAIL만 준비 실패로 센다.
    Write-Output ('[{0,-4}] {1}' -f $State, $Message)
    if ($State -eq 'FAIL') { $script:Problems++ }
}

# 1. 저장소 경로
$DroneRepo = Get-DroneRepoRoot
Write-Check 'OK' "Unreal 저장소: $DroneRepo"
$MdRepoPath = Get-MdRepoRoot $MdRepo
if ($MdRepoPath) { Write-Check 'OK' "md 저장소: $MdRepoPath" }
else { Write-Check 'FAIL' 'md 저장소를 찾지 못했다. -MdRepo <경로> -WriteLocalConfig 로 등록한다.' }

# 2. Git으로 공유돼야 하는 파일이 실제로 추적되는지(다른 PC 연동의 전제)
$Shared = @('CLAUDE.md', 'AGENTS.md', '.mcp.json', '.claude/settings.json',
    '.claude/skills/codex-handoff/SKILL.md', '.claude/codex-bridge/Invoke-CodexTask.ps1',
    '.claude/codex-bridge/BridgeCommon.ps1', '.claude/codex-bridge/roles/common.md')
foreach ($Rel in $Shared) {
    $Full = Join-Path $DroneRepo $Rel
    if (-not (Test-Path -LiteralPath $Full)) { Write-Check 'FAIL' "없음: $Rel (Pull 했는지 확인)"; continue }
    $Tracked = & git -C $DroneRepo ls-files -- $Rel
    if ($Tracked) { Write-Check 'OK' "공유됨: $Rel" }
    else { Write-Check 'WARN' "아직 Git 미추적: $Rel — Commit/Push해야 다른 PC에 따라간다" }
}

# 3. Codex CLI와 로그인(auth.json은 PC마다 각자 로그인한다. 복사하지 않는다)
$Codex = Find-CodexExe
if (-not $Codex) {
    Write-Check 'FAIL' 'codex.exe 없음 — Codex 데스크톱 앱을 설치하고 ChatGPT 계정으로 로그인한다.'
} else {
    $Version = (& $Codex --version) -join ' '
    Write-Check 'OK' "Codex CLI: $Version ($Codex)"
    $PreviousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'   # login status는 결과를 stderr로 내보낸다.
    $Login = (& $Codex login status 2>&1 | ForEach-Object { "$_" }) -join ' '
    $ErrorActionPreference = $PreviousPreference
    if ($Login -match 'Logged in') { Write-Check 'OK' "Codex 로그인: $Login" }
    else { Write-Check 'FAIL' "Codex 로그인 안 됨: $Login — Codex 앱에서 로그인한다." }
}

# 4. Claude 쪽(로그인 자체는 데스크톱 앱에서 확인한다)
$UserSettings = Join-Path $env:USERPROFILE '.claude\settings.json'
if (Test-Path -LiteralPath $UserSettings) { Write-Check 'OK' "Claude 사용자 설정: $UserSettings" }
else { Write-Check 'WARN' 'Claude 사용자 설정(~/.claude/settings.json) 없음 — md 저장소 docs/git/CLAUDE_CODEX_SETUP.md의 권장값을 넣으면 모델·권한 기본값이 맞춰진다(선택).' }

# 사용자 공통 규칙의 공유 원본은 md 저장소 docs/git/USER_RULES.md다(2026-10-01 drone에서 이동).
$UserClaudeMd = Join-Path $env:USERPROFILE '.claude\CLAUDE.md'
$UserRules = if ($MdRepoPath) { [IO.Path]::Combine($MdRepoPath, 'docs\git\USER_RULES.md') } else { $null }
if (-not $UserRules -or -not [IO.File]::Exists($UserRules)) {
    Write-Check 'FAIL' 'md 저장소의 docs/git/USER_RULES.md가 없다 — md 저장소를 Pull한다.'
} else {
    $ImportLine = "@$UserRules"
    $Current = if (Test-Path -LiteralPath $UserClaudeMd) { [IO.File]::ReadAllText($UserClaudeMd, $Utf8) } else { '' }
    # 이전 위치(drone .claude/codex-bridge/USER_RULES.md 등)를 가져오는 줄이 남아 있으면 낡은 설치로 본다.
    $StaleLines = @($Current -split "`r?`n" | Where-Object { $_ -match '^@.*USER_RULES\.md\s*$' -and $_.Trim() -ne $ImportLine })
    if ($Current.Contains($ImportLine) -and $StaleLines.Count -eq 0) {
        Write-Check 'OK' "사용자 공통 규칙 설치됨: $UserClaudeMd ← $UserRules"
    } elseif (-not $InstallUserRules) {
        Write-Check 'WARN' '사용자 공통 규칙 미설치 또는 이전 경로 — -InstallUserRules 로 ~/.claude/CLAUDE.md를 맞춘다.'
    } else {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $UserClaudeMd) | Out-Null
        # 이전 경로 줄만 새 줄로 바꾸고, 사용자가 직접 쓴 다른 지침은 그대로 둔다.
        $Lines = @($Current -split "`r?`n" | Where-Object { $StaleLines -notcontains $_ })
        if (-not ($Lines -contains $ImportLine)) {
            if (-not $Current) { $Lines = @('# 사용자 전역 지침 (정규연)', '') }
            $Lines += @('', '# 사용자 공통 규칙(정규연) — 공유 원본: md 저장소 docs/git/USER_RULES.md', $ImportLine)
        }
        [IO.File]::WriteAllText($UserClaudeMd, (($Lines -join "`n").TrimEnd() + "`n"), $Utf8)
        Write-Check 'OK' "설치: $UserClaudeMd ← $ImportLine (이전 경로 $($StaleLines.Count)줄 교체)"
    }
}

$LocalJson = Join-Path $PSScriptRoot 'local.json'
$LocalSettings = Join-Path $DroneRepo '.claude\settings.local.json'
if (Test-Path -LiteralPath $LocalSettings) { Write-Check 'OK' 'PC 전용 Claude 설정 있음: .claude/settings.local.json' }
else { Write-Check 'WARN' 'PC 전용 Claude 설정 없음 — -WriteLocalConfig 로 md 저장소를 추가 작업 폴더로 등록한다.' }

# 5. PC 전용 설정 쓰기
if ($WriteLocalConfig) {
    if (-not $MdRepoPath) { throw 'md 저장소 경로가 없어 PC 전용 설정을 쓸 수 없다.' }

    $Local = [ordered]@{ mdRepo = $MdRepoPath; computer = $env:COMPUTERNAME; updated = (Get-Date).ToString('s') }
    [IO.File]::WriteAllText($LocalJson, ($Local | ConvertTo-Json), $Utf8)
    Write-Check 'OK' "기록: $LocalJson"

    $Settings = if (Test-Path -LiteralPath $LocalSettings) {
        Get-Content -LiteralPath $LocalSettings -Raw -Encoding UTF8 | ConvertFrom-Json
    } else { New-Object PSObject }
    if (-not $Settings.PSObject.Properties['permissions']) {
        $Settings | Add-Member -NotePropertyName permissions -NotePropertyValue (New-Object PSObject)
    }
    $Dirs = @()
    if ($Settings.permissions.PSObject.Properties['additionalDirectories']) { $Dirs = @($Settings.permissions.additionalDirectories) }
    if ($Dirs -notcontains $MdRepoPath) { $Dirs += $MdRepoPath }
    $Settings.permissions | Add-Member -NotePropertyName additionalDirectories -NotePropertyValue $Dirs -Force
    [IO.File]::WriteAllText($LocalSettings, ($Settings | ConvertTo-Json -Depth 8), $Utf8)
    Write-Check 'OK' "기록: $LocalSettings (additionalDirectories에 md 저장소)"
}

# 6. 참고: Editor가 켜져 있으면 Claude의 C++ Build가 거절된다(Live Coding).
if (Get-Process -Name 'UnrealEditor*' -ErrorAction SilentlyContinue) {
    Write-Check 'WARN' 'Unreal Editor 실행 중 — Claude가 Build하려면 저장 후 종료해야 한다.'
}

if ($Problems -eq 0) { Write-Output 'COLLAB_READY' } else { Write-Output "COLLAB_NOT_READY ($Problems)"; exit 1 }
