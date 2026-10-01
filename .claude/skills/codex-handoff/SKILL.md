---
name: codex-handoff
description: 코드 외 작업(md 문서·Drone Space 최신화, Figma/Trello 대조, 보고서, Claude 변경 교차 리뷰, 원인 조사 의견)을 Codex에게 맡기고 결과를 검증한다. 코드 작업을 마무리했을 때, 사용자가 "최신화"·"문서 정리"·"Space 갱신"·"코덱스한테 맡겨"·"교차 리뷰"를 요청했을 때 사용한다.
---

# Codex에게 맡기기

역할 분담: **코드(Source/Drone)·Build·자동화 테스트는 Claude**, 그 밖의 문서·Space·대조·보고서·리뷰 의견은 **Codex**.
Codex는 `.claude/codex-bridge/Invoke-CodexTask.ps1`로 부르며 샌드박스 때문에 Unreal 저장소를 고칠 수 없다.

## 1. 역할 고르기

| Role | 언제 | 샌드박스 |
|---|---|---|
| `docs` | STATUS/WORKBOARD/WORKLOG·가이드 갱신, Drone Space 갱신, 보고서 작성 | md 저장소만 쓰기 |
| `review` | 미커밋 diff 교차 리뷰 | 읽기 전용 |
| `research` | 회귀 원인 의견, Figma/웹 대조, 기획 선택지 정리 | 읽기 전용 |

## 2. 지시서 쓰기 — `.claude/codex-bridge/briefs/<날짜>-<이름>.md`

Codex는 이 대화를 모른다. `.claude/codex-bridge/BRIEF_TEMPLATE.md`를 복사해 **모든 칸을 사실로** 채운다(결과 없음 = "미실행/미확인", 파일 없음 = "현재 PC에 없음"). 전체 규칙은 md 저장소 `docs/git/CLAUDE_CODEX_COLLABORATION.md`.

- 기준: Unreal 커밋 해시, 미커밋 변경 파일, PC(C PC), 작업 도구(Claude)
- 한 일: 파일별 변경 요약
- 검증: 명령·테스트 이름·수치. 구현됨 / 자동 검증됨 / 수동 확인 대기 / 미구현으로 분류
- 문서에 반영할 위치(STATUS·WORKBOARD 카드 ID·WORKLOG·가이드)와 Space 포함 여부
- 하지 말 것(예: "Figma 표기 충돌은 현재 미정으로만 기록")

지시서에 API 키·토큰·개인 정보를 넣지 않는다.

## 3. 실행

```powershell
& 'C:\URproject\drone\.claude\codex-bridge\Invoke-CodexTask.ps1' -Role docs -BriefFile 'C:\URproject\drone\.claude\codex-bridge\briefs\<이름>.md' -Name <영문-이름>
```

- `-Effort`: 기본 `medium`. 리뷰·원인 조사는 `high`. `xhigh`는 사용자가 원할 때만(사용량 큼).
- `-Network`: docs에서 Trello·웹을 읽어야 할 때만.
- docs 작업은 몇 분 걸릴 수 있으니 PowerShell `run_in_background`로 돌리고 완료 알림을 기다린다.
- 호출마다 사용자의 ChatGPT/Codex 사용량이 든다. 같은 내용을 반복 위임하지 않는다.

## 4. 결과 검증 — 그대로 믿지 않는다

1. `runs/<...>/result.md`와 `meta.json`의 `exitCode`를 읽는다.
2. docs: `git -C <md 저장소> status --short`와 `git diff`로 실제 변경을 확인하고, 지시서에 없는 사실·수치가 들어갔는지 본다. 틀리면 고쳐 달라고 다시 맡기거나 사용자에게 보고한다.
3. review/research: 지적마다 코드·로그로 직접 확인한 뒤 맞는 것만 반영한다.
4. "Claude에게 넘길 코드 작업"이 있으면 Claude가 이어서 처리하거나 사용자에게 제안한다.

## 5. 사용자 보고

Codex가 한 일과 Claude가 검증한 결과를 구분해 쓴다. Space 반영 여부, md 저장소 미커밋 상태, Commit/Push를 하지 않았음을 밝힌다.
