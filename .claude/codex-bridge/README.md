# Claude ↔ Codex 브리지

Claude Code가 코드 외 작업을 Codex CLI(`codex exec`)에게 맡기는 도구다. 규칙·세팅·사용자 규칙 문서는 md 저장소 `docs/git/`(`CLAUDE_CODEX_COLLABORATION.md`, `CLAUDE_CODEX_SETUP.md`, `USER_RULES.md`)에 있고, Claude 쪽 절차는 `.claude/skills/codex-handoff/SKILL.md`.

| 경로 | 내용 | Git |
|---|---|---|
| `Invoke-CodexTask.ps1` | 역할별 샌드박스로 Codex 실행, 결과 저장 | 공유 |
| `BridgeCommon.ps1` | PC별 저장소·codex.exe 위치 찾기 | 공유 |
| `Test-CollabSetup.ps1` | PC 준비 점검, `-WriteLocalConfig`로 PC 전용 설정 생성 | 공유 |
| `roles/common.md`, `docs.md`, `review.md`, `research.md` | 역할 계약(`{{DRONE_REPO}}`·`{{MD_REPO}}`는 실행 때 치환) | 공유 |
| `BRIEF_TEMPLATE.md` | 지시서 틀 | 공유 |
| `COLLABORATION.md`, `SETUP.md`, `USER_RULES.md` | md 저장소로 옮겼다는 안내만 | 공유 |
| `local.json` | 이 PC의 md 저장소 경로 | 제외 |
| `briefs/` | Claude가 쓰는 지시서 | 제외 |
| `runs/<시각-역할-이름>/` | `prompt.md`, `result.md`, `stdout.log`, `stderr.log`, `meta.json` | 제외 |

## 실행 기록 (C PC)

| 날짜 | 실행 | 결과 |
|---|---|---|
| 2026-10-01 | research `smoke-tools` | 종료 0. Codex 보고 도구: Spaces 읽기·수정, Figma, 브라우저, 웹 검색. Trello 전용 도구 없음 |
| 2026-10-01 | docs `docs-sync` | 종료 0. md 8파일 갱신, Claude가 diff 검증. Space 저장은 `approval policy is never`로 실패 → `--approve-for-me` 추가 |
| 2026-10-01 | review `review-lobby` | 종료 0. 지적 2건(줄바꿈 재적용, WBP 스크롤바) — Claude 확인 후 반영 |
| 2026-10-01 | research `codex-rules` | 종료 0. 문서·Space·지시서·카드·다른 PC·충돌 규칙 6절 → COLLABORATION.md에 합침 |
| 2026-10-01 | docs `docs-collab` | 첫 시도 종료 2(`--approve-for-me`와 `-s` 동시 사용 불가) → 수정 후 종료 0. md 10파일, **Space 3페이지 저장·재조회 성공**. Claude가 diff 검증 |
