# CLAUDE.md — Drone Unreal 프로젝트 (gyeonliz/drone)

Codex에서 Claude로 이관됐다(2026-10-01). 기존 Codex 지침은 그대로 유효하며 아래에서 불러온다.

@AGENTS.md

## 문서 저장소 위치 (PC마다 다름)

AGENTS.md의 "인접 md 저장소(`../md`)"는 PC마다 경로가 다르다. 이 PC의 실제 경로는 `.claude/codex-bridge/local.json`의 `mdRepo`(Git 제외)에 있다. 없으면 `.claude/codex-bridge/Test-CollabSetup.ps1 -WriteLocalConfig`로 만든다(절차: md 저장소 `docs/git/CLAUDE_CODEX_SETUP.md`).

| PC | Unreal | md |
|---|---|---|
| C PC | `C:\URproject\drone` | `C:\Users\jkw11\Documents\Codex\2026-08-19\codex-gpt-chatgpt-codex-1-6` |
| D PC(이전 작업컴) | `D:\JGY\project\drone` | `D:\JGY\project\md` |

작업 전 그 폴더의 `WORK_PC_START_HERE.md`, `CONTEXT.md`, `STATUS.md`, `WORKBOARD.md`를 읽고 현재 Git 상태와 대조한다.

## 핵심 경계 (요약 — 상세는 md 저장소 CONTEXT.md)

- 엔진: UE 5.8 (`C:\Program Files\Epic Games\UE_5.8`). 솔루션은 `Drone.sln`.
- 생산 코드는 `Source/Drone`, 자산은 `/Game/Drone`. ThirdPerson·Combat·Platforming·SideScrolling은 Legacy이므로 상속·참조하지 않는다.
- `/Game/Drone/Maps/Lvl_DroneTraining`은 팀원 Production 맵이다. 저장·덮어쓰기·재생성 금지. 기능 검증은 `/Game/Drone/Maps/TestMap/...`에서 한다.
- C++는 상태·규칙·계약, 조정 수치와 자산 연결은 Blueprint/Data Asset(`EditDefaultsOnly`/`EditAnywhere` + Category)에 둔다.
- 전체 C++ Build 전에 Editor를 저장하고 종료한다. 단순 문서 최신화에는 Build·PIE·맵 생성 도구를 실행하지 않는다.
- 자산 이동은 Unreal AssetTools로 하고 Redirector·Soft Reference를 검증한다. 탐색기로 옮기지 않는다.
- 구현됨 / 자동 검증됨 / 수동 확인 대기 / 미구현을 구분해 보고하고, 다른 PC의 결과를 이 PC 결과처럼 쓰지 않는다.

## 응답·작업 방식 (Codex 대화에서 이관, 2026-10-01)

Codex 세션 기록(2026-08-12~2026-10-01, 드론 관련 58스레드·사용자 메시지 187건)에서 반복 확인된 사용자 요구다. 원본은 `C:\Users\jkw11\.codex`에 남아 있고 이 PC의 Claude 메모리에도 요약돼 있다.

- 한국어로 답하고 Commit 메시지도 한국어로 쓴다.
- **정확성 우선.** 확인하지 않은 기능을 있는 것처럼 말하지 않는다. 사용자가 확정하지 않은 사항(군/국가 설정, 적군 국적, J3C 실제 협력 관계, 최종 드론 종류·게임 규칙·멀티플레이 방식·세부 입력·최종 물리)은 "현재 미정"으로 표시하고 임의로 확정하지 않는다.
- 사용량(토큰)을 아낀다. 전체 문서 재독·반복 동기화·불필요한 대규모 탐색 없이 필요한 범위만 읽는다. (2026-09-02 사용자 지적)
- 한 번에 여러 시스템을 만들지 않는다. 기능 단위로 완성·테스트한 뒤 다음으로 넘어가고, 작업은 1~3시간 크기로 쪼갠다.
- 새 시스템 설명 순서: ① 왜 필요한지 → ② 담당 클래스 → ③ 헤더 추가 → ④ CPP 추가 → ⑤ Blueprint 설정 → ⑥ Editor 테스트 방법 → ⑦ 정상 결과 → ⑧ 문제 시 확인 항목.
- 코드·Blueprint에 사용자가 읽기 쉬운 주석을 남기고, 조정 수치(사격 산포 각도, 체력, 기상 세기 등)는 Blueprint/Data Asset에서 조정 가능하게 노출한다.
- Unreal 기초 개념(C++ Character, Enhanced Input, Camera/SpringArm, Weapon, Line Trace, HP/Damage, Montage, Monster AI, UMG, Item, GameMode, Timer, DataTable, Collision, Live Coding)은 경험이 있으므로 매번 처음부터 장황하게 설명하지 않는다. IDE는 Visual Studio 2022다.
- Figma `Project Droner`는 **읽기 전용**이다. 내부 내용을 수정하지 않는다. Trello도 읽기 참고이며 카드를 바꾸지 않는다.
- "진행해/쭉 해줘/남은 것도"는 실제 작업 지시다. 할 수 있는 구현·연결·설정을 사용자에게 떠넘기지 않고, 검토할 만한 완성도를 갖춘 뒤 보고한다. 사용자가 "알아서 정해"라고 위임한 값(예: 2026-09-03 체력 100)은 정하되 기본값과 근거를 적는다.
- 검증은 기능이 실제로 연결된 맵·화면·입력에서 한다. 예: Smart Object NPC 문제는 `Lvl_NPCSmartObjectGreybox`에서 확인하고, Shotgun 전용 맵 통과로 대신하지 않는다(2026-09-17 지적). UI는 패드 입력·뒤로가기·성능까지 본다.
- 패키징·전체 쿠킹처럼 PC를 10분 넘게 무겁게 쓰는 작업은 "싹 해줘" 안이라도 예상 시간·영향을 말하고 실행 시점을 먼저 확인받는다(2026-10-02 무단 패키징으로 렉 발생 지적). 평소 Build·자동화 테스트는 그대로 진행한다.
- 사용자 공통 규칙(정확성·직접 진행·실제 검증·이미지·세계관)의 공유 원본은 md 저장소 `docs/git/USER_RULES.md`이며 각 PC의 `~/.claude/CLAUDE.md`에 설치한다.

## Claude ↔ Codex 협업 (2026-10-01)

- **Claude 담당**: `Source/Drone` C++, Build, 자동화 테스트, Unreal 도구 실행, 코드 수정.
- **Codex 담당**: md 문서 저장소 최신화, Drone Space 갱신, Figma·Trello 대조, 보고서·제출 서류, Claude 변경의 교차 리뷰·원인 조사 의견.
- 전체 규칙: md 저장소 `docs/git/CLAUDE_CODEX_COLLABORATION.md`. 다른 PC 세팅: md 저장소 `docs/git/CLAUDE_CODEX_SETUP.md`. (이 저장소 `.claude/codex-bridge/`에는 스크립트·역할 계약·지시서 틀만 둔다.)
- 코드 작업을 마무리하거나 사용자가 최신화·문서·Space·교차 리뷰를 요청하면 `codex-handoff` 스킬 절차로 `.claude/codex-bridge/Invoke-CodexTask.ps1`을 써서 Codex에게 맡긴다. Codex는 샌드박스상 이 저장소를 고칠 수 없다.
- Codex 결과는 그대로 믿지 않는다. md 저장소 `git diff`와 코드·로그로 확인한 뒤 사용자에게 Codex가 한 일과 Claude 검증 결과를 나눠 보고한다.
- 호출마다 사용자의 Codex 사용량이 든다. 기본 추론 강도는 `medium`이며 같은 위임을 반복하지 않는다.

## Claude 환경 메모

- Codex에서 쓰던 Unreal MCP(`http://127.0.0.1:8000/mcp`)는 루트 `.mcp.json`으로 등록한다. Editor 쪽 MCP 서버가 켜져 있을 때만 연결된다.
- Drone Space(ChatGPT Pages)는 Claude 도구로 직접 접근할 수 없다. Space 갱신은 Codex(`docs` 역할)에게 맡기고, Codex도 실패하면 미반영 범위를 보고한다.
- Commit/Push는 사용자가 요청할 때만 한다. `.claude/settings.json`을 두면 commit/push와 Production Training 맵 수정 시 승인 요청이 뜬다.
- Codex 설정은 `C:\Users\jkw11\.claude\settings.json`으로 이관했다. `model: opus`(Codex `gpt-6.1-sol`/`xhigh` 대응), `defaultMode: acceptEdits`(Codex `approval_policy=never` + `danger-full-access` 대응이지만 Commit/Push·되돌리기류는 승인 요청으로 남김), `auth.json`·`.env` 읽기 차단이다.
- 문서 저장소 경로는 PC 전용 `.claude/settings.local.json`의 `permissions.additionalDirectories`에 등록한다(Git 제외, Codex sandbox writable root 대응). 공유 `.claude/settings.json`에는 PC 경로를 넣지 않는다.
- 사용자 홈의 `~/.claude/settings.json`과 Claude 메모리는 PC별이라 다른 PC로 따라가지 않는다. 지켜야 할 규칙은 이 파일과 md 저장소 `docs/git/CLAUDE_CODEX_COLLABORATION.md`에 둔다.
- Codex의 플러그인(chrome·browser·visualize·documents/pdf/spreadsheets/presentations·computer-use)은 Claude 내장 도구·스킬로 대체된다. `node_repl` MCP는 Codex 전용 런타임이라 이관하지 않았다.
