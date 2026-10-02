<#
    Invoke-CodexTask.ps1, Test-CollabSetup.ps1이 같이 쓰는 경로 찾기.
    PC마다 저장소 위치가 달라서(C PC: C:\URproject\drone + Documents\Codex\...\codex-gpt-chatgpt-codex-1-6,
    D PC: D:\JGY\project\drone + D:\JGY\project\md) 경로를 코드에 박지 않는다.
#>

# Unreal 저장소 = 이 파일 기준 두 단계 위(.claude\codex-bridge → 저장소 루트).
function Get-DroneRepoRoot {
    return (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
}

function Test-MdRepo([string]$Path) {
    # Join-Path는 없는 드라이브(C PC의 D:)에서 예외를 내므로 .NET 경로 결합을 쓴다.
    if (-not $Path) { return $false }
    return [IO.File]::Exists([IO.Path]::Combine($Path, 'STATUS.md')) -and [IO.File]::Exists([IO.Path]::Combine($Path, 'WORKBOARD.md'))
}

# md 문서 저장소 찾는 순서: 인자 → 환경변수 DRONE_MD_REPO → local.json(이 PC 전용, Git 제외) → 알려진 후보.
function Get-MdRepoRoot([string]$Override) {
    $LocalJson = Join-Path $PSScriptRoot 'local.json'
    $Candidates = @()
    if ($Override) { $Candidates += $Override }
    if ($env:DRONE_MD_REPO) { $Candidates += $env:DRONE_MD_REPO }
    if (Test-Path -LiteralPath $LocalJson) {
        $Local = Get-Content -LiteralPath $LocalJson -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($Local.mdRepo) { $Candidates += $Local.mdRepo }
    }
    $DroneParent = Split-Path -Parent (Get-DroneRepoRoot)
    $Candidates += @(
        (Join-Path $DroneParent 'md'),                                                    # D PC 형태: ...\project\drone + ...\project\md
        'D:\JGY\project\md',
        (Join-Path $env:USERPROFILE 'Documents\Codex\2026-08-19\codex-gpt-chatgpt-codex-1-6')  # C PC
    )
    foreach ($Candidate in $Candidates) {
        if (Test-MdRepo $Candidate) { return (Resolve-Path -LiteralPath $Candidate).Path }
    }
    return $null
}

# Codex CLI: PATH → 데스크톱 앱이 설치한 최신 codex.exe(앱 업데이트 때 폴더 해시가 바뀐다).
function Find-CodexExe {
    $OnPath = (Get-Command codex -ErrorAction SilentlyContinue).Source
    if ($OnPath) { return $OnPath }
    return Get-ChildItem -Path "$env:LOCALAPPDATA\OpenAI\Codex\bin\*\codex.exe" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName
}
