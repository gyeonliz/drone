# Drone 프로젝트 작업 지침

- 문서 기준은 인접 md 저장소의 STATUS.md·WORKBOARD.md다. 문서 저장소를 찾지 못하면 현재 PC 경로를 확인하고 오래된 상태를 추정하지 않는다.
- 관련 개발 작업 마무리와 최신화 때 MD와 Drone Space의 관련 페이지를 함께 갱신한다. 대상/절차는 md 저장소의 docs/git/DRONE_SPACES_SYNC.md(PC별 md 경로는 CLAUDE.md 표·`.claude/codex-bridge/local.json`)이며 안내 페이지는 https://chatgpt.com/space/page_75732d3acd6481919ba26bf5b0b972cd 이다.
- 구현·자동 검증·수동 확인·미구현을 분리하고 다른 PC의 실행 결과를 현재 PC의 결과처럼 쓰지 않는다. Spaces 접근 불가 시 로컬 기록과 미반영 범위를 남긴다.
- 새 생산 코드는 Source/Drone, 프로젝트 소유 자산은 /Game/Drone에 둔다. Legacy 신규 참조/상속을 만들지 않는다.
- 팀원 Production /Game/Drone/Maps/Lvl_DroneTraining을 시험용으로 저장·덮어쓰기·재생성하지 않는다. 기능 검증은 TestMap에서 한다.
- C++ 전체 Build 전에 Editor를 저장 후 종료한다. 단순 상태 최신화에 Build·PIE·맵 생성 도구를 실행하지 않는다.
- Trello는 읽기 참고이며 카드 수정, Git Commit/Push, 공유 권한 변경, 예약 자동화는 별도 사용자 지시 없이 하지 않는다.
- 역할 분담(정규연 작업 환경, 2026-10-01~): 이 저장소의 C++·Build·자동화 테스트·Unreal 도구 실행은 Claude Code가 맡고, Codex는 md 문서·Drone Space·Figma/Trello 대조·보고서·교차 리뷰를 맡는다. Codex가 코드 변경 요청을 받으면 직접 고치지 말고 WORKBOARD에 카드(변경 대상·완료 조건)로 남긴 뒤 Claude Code에서 진행하도록 안내한다. Claude가 맡긴 작업의 계약은 `.claude/codex-bridge/roles/common.md`, 전체 협업 규칙은 md 저장소 `docs/git/CLAUDE_CODEX_COLLABORATION.md`를 따른다.
