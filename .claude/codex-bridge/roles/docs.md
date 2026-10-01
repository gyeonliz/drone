# 역할: docs — 문서·Space 최신화

샌드박스: md 저장소만 쓰기 가능(`workspace-write`), 나머지는 읽기 전용.

1. md 저장소의 `AGENTS.md`, `STATUS.md`, `WORKBOARD.md` 현재 내용을 지시서와 관련된 부분만 읽는다.
2. 지시서의 사실(변경 파일, 테스트 수치, 판정)을 기준으로 갱신한다.
   - 현재 상태 → `STATUS.md`
   - 작업 순서·카드 → `WORKBOARD.md`
   - 날짜별 수행 이력 → `docs/history/DRONE_WORKLOG.md` (끝에 추가)
   - 주제별 가이드가 지정되면 해당 `docs/...` 문서
3. 진행 상황을 기록하려고 새 파일을 만들지 않는다. 지시서가 새 파일을 명시한 경우만 만든다.
4. 기존 기록(특히 "Codex" 표기가 있는 과거 이력)은 바꾸지 않는다. 새 기록의 작업 도구는 지시서에 적힌 대로(Claude 또는 Codex) 쓴다.
5. Drone Space 갱신은 `docs/git/DRONE_SPACES_SYNC.md`의 대상·절차를 따른다. 지시서가 "Space 제외"라고 하면 하지 않는다. 도구가 없거나 실패하면 로컬만 갱신하고 미반영으로 보고한다.
6. 끝나면 `git -C <md 저장소> status --short`로 실제 바뀐 파일을 확인해 보고에 적는다. Commit은 하지 않는다.
