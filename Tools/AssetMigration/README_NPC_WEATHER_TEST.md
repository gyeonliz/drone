# NPC / Weather 안전 시험 절차

2026-09-17. 저장 자산과 Production 맵을 수정하지 않는 개발용 절차다.

## 환경 / 경계

- 프로젝트: `C:\URproject\drone\Drone.uproject`.
- 요청 기준은 UE 5.8.1이나 이 PC의 `C:\Program Files\Epic Games\UE_5.8\Engine\Build\Build.version`은 **5.8.2 / CL 56702186**이다. 5.8.1 검증으로 간주하지 않는다.
- Production Training/실제 플레이 맵은 열거나 저장하지 않는다. 아래 명령은 시작 맵도 명시한다.
- 새 Niagara/Material/맵 저장, 외부 에셋 다운로드, MCP 설치·연결은 하지 않았다.
- 기존 `Invoke-DroneWeatherTestMap.ps1 -Mode Validate`도 내부에서 BP 보장/컴파일을 하고 자산이 없으면 생성한다. 완전 읽기 전용 감사로 간주하지 말고, 이 작업에서는 실행하지 않는다.

## Weather: 맵 저장 없이 비 Snapshot과 디버그 프리뷰 시험

1. 최신 DroneEditor 빌드 후 `/Game/Drone/Maps/TestMap/Lvl_DroneWeatherSystemsTest`만 연다.
2. PIE 시작 후 뷰포트에 키보드 포커스를 준다. 기존 기본값은 LightWind다.
3. 숫자열 **9**: `RainStorm_Greybox`, **7**: `Clear`, **8**: `LightWind`. NumPad 7/8/9는 지원하지 않는다.
4. RainStorm에서는 화면 `Rain / Spawn / Wet` 값에 비례한 임시 디버그 선분이 뷰 주변에 나타나고, 바람이 선분의 수평 이동 방향에 반영되는지 본다. Storm은 Rain 0.8, Clear는 Rain/Spawn/바람 0, LightWind는 비 없이 바람이 남아야 한다.
5. 기존 **1/2/3**(NumPad 포함)으로 Easy/Manual/Rate-Acro 보정 차이를 비교한다.
6. 종료 후 맵·BP·Profile을 저장할 필요가 없다. 키는 World Snapshot만 즉시 바꾼다. Profile 전환의 자연스러운 보간 평가는 별도로 `ApplyWeatherProfile(Profile, false)`를 사용한다.

`ADroneWeatherDebugVisualizer::ApplyTestWeatherPreset(0/1/2)`가 같은 진입 경로다. 전용 Weather 맵 이름(PIE prefix 포함)이 아니거나 인덱스가 잘못되면 변경 없이 false를 반환한다. Blueprint 인스턴스 `Enable Weather Preset Hotkeys`로 키를 끌 수 있다.

**보이는 선분은 TestMap 전용 디버그 프리뷰일 뿐, 실제 Niagara 강우 효과가 아니다.** 프리뷰는 Snapshot의 RainIntensity×RainSpawnScale을 최대 80개 선분으로 제한하고 0.2초마다 갱신한다. 비가 0이면 추가 DrawDebugLine 호출을 멈추고 남은 선분은 최대 약 0.3초 뒤 사라진다. 물리·충돌 트레이스는 하지 않으며 Weather TestMap 밖에서는 실행되지 않는다. 이 방식의 `stat gpu` 수치는 Niagara 비 성능을 대표하지 않는다. Niagara system/material/parameter binding, Wetness MPC consumer, Rain Audio 연결은 아직 없다.

## 비 표현 후속 연결 계약 (미구현)

정식 구현은 `Weather Profile → WorldSubsystem Snapshot/event (기본 10Hz) → local camera rain presenter → Niagara user parameters` 순서다. 공유 wind는 cm/s 벡터, rain intensity와 spawn scale은 0~1이다. Gameplay 풍속/난수/피해/신호를 품질 설정으로 변경하지 않는다.

- 정식 Niagara에서는 Local camera당 하나, 좁은 spawn volume, GPU sprite 우선. Camera 이동만 프레임별 추종하고 wind/spawn 값은 Snapshot 변경 때 갱신한다. 현재 적용된 디버그 프리뷰는 전용 맵 한정, 최대 80개, 5Hz, 비가 없으면 추가 draw를 하지 않는다.
- Intensity 또는 spawn scale이 0이면 spawn 중지, 잔여 파티클 종료 뒤 component tick/deactivate. 종료 여부를 stat/debugger로 확인한다. 현재 비 component는 없어 이 계약을 구현했다고 주장하지 않는다.
- Particle별 trace 금지. 실내 volume/차폐 서비스가 없으므로 실내 감쇠는 미구현이다. Snapshot의 attenuation 값만으로 지붕 검출을 했다고 간주하지 않는다.
- OilRig Material Function은 Niagara emitter/system 자체가 아니다. Wetness MPC도 consumer material이 실제로 읽어야 효과가 난다.

다음 숫자는 **향후 authoring 시작값/비교 조건이지 적용된 preset이나 확정 예산이 아니다**. Bounds는 lifetime × 최대 wind와 FPV 최대 이동까지 포함해 다시 검증해야 한다.

| 품질 | spawn 배율 | sprite 길이/폭 배율 | 카메라 volume 반경 | cull 거리 | fixed bounds 반경 후보 | splash |
|---|---:|---:|---:|---:|---:|---|
| Low | 0.25 | 0.7 | 8m | 20m | 25m | Off |
| Medium | 0.5 | 0.85 | 10m | 25m | 30m | Off |
| High | 0.75 | 1.0 | 12m | 30m | 35m | 후속 제한형 |
| Epic | 1.0 | 1.0 | 15m | 35m | 40m | 후속 제한형 |

실제 screen size는 world sprite 크기뿐 아니라 해상도/FOV에 따라 달라지므로 화면 캡처에서 픽셀 길이·폭과 overdraw를 같이 기록한다. 고정 bounds와 cull 거리가 서로 충돌해 이펙트가 깜빡이지 않는지 회전/고속 이동에서 확인한다.

## 성능 비교 (아직 측정 안 함)

렌더링 가능한 Editor/standalone에서 같은 PC·해상도·FOV·품질·카메라 경로를 고정한다. `-nullrhi` 자동화는 GPU 성능/비 외형 검증에 사용할 수 없다.

1. 셰이더 준비 후 30초 warm-up, 60초씩 3회 측정. VSync와 FPS cap 상태를 기록한다.
2. Clear / LightWind / RainStorm 각각, 정지 / 27m/s 직선 / 빠른 Roll 비교. 품질별로 반복한다. wind를 동일하게 둔 rain-off/on 비교도 필요하다.
3. `stat unit`, `stat gpu`, `stat niagara`, `profilegpu`와 Niagara Debugger에서 Game/Draw/GPU ms, Niagara/Translucency ms, 활성 system/emitter/particle 수, spikes를 기록한다. 평균과 p95는 Unreal Insights/캡처로 수집하며 눈대중을 수치로 쓰지 않는다.
4. Clear 대비 GPU delta와 overdraw, 화면 가림을 비교한다. 비 전체 GPU ≤1ms는 기존 계획의 **제안**일 뿐 합격 인증/확정 예산이 아니다.
5. rain-off 후 active spawn/tick=0, system 중복 없음, Low에서도 wind Snapshot 동일 확인. 실내 테스트는 실제 차폐 구현 후에만 수행한다.

## NPC 최소 재현 / 판단

- `/Game/Drone/Maps/TestMap/Lvl_DroneShotgunSystemsTest`: 사거리 안/밖 경계를 반복 이동, 0.2초 미만 밖 흔들림과 지속 이탈 구분, ±1.9° 정면 흔들림, 이동 방향과 몸/Gaze 정렬 확인.
- `/Game/Drone/Maps/TestMap/Lvl_NPCSmartObjectGreybox`: 순찰→감지→MG 경합→개인화기/엄폐→사수 사망 교대→Lost/Search→순찰 복귀.
- 현재 기준: 실제 3D 사거리 안 즉시 정지·사격, 밖 0.2초 지속 후 추적, 이동 회전은 CharacterMovement 소유, 정지 몸 회전은 3° stop/6° start, StateTree 복귀는 다음 Tick.
- 새 불안정 원인이 이 회귀에서 입증되지 않으면 AI 코드를 추측 수정하지 않는다. Headless 성공은 실제 AnimBP pose 중첩/화면 떨림 해소 증명이 아니다.
- 사용자 재현에 필요한 기록: NPC BP/맵, 시작 위치·표적 거리/높이, 순찰/점유 상태, FPS, 발생까지 조작, 10~20초 영상, 재현 빈도. 실제 플레이 맵은 저장하지 않고 별도 시험 맵에서 재현한다.

## 빌드 / 자동화 명령

Editor를 닫고 PowerShell에서 실행한다. 인증정보는 인수에 넣지 않는다.

```powershell
$ue = 'C:\Program Files\Epic Games\UE_5.8'
$project = 'C:\URproject\drone\Drone.uproject'
& "$ue\Engine\Build\BatchFiles\Build.bat" DroneEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE

& "$ue\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $project /Game/Drone/Maps/TestMap/Lvl_DroneWeatherSystemsTest -unattended -nop4 -nullrhi -nosound '-DisablePlugins=ModelContextProtocol,EditorToolset,AutomationTestToolset,UMGToolSet,StateTreeToolset,AIModuleToolset' '-ExecCmds=Automation RunTests Drone.Weather' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=C:\URproject\drone\Saved\Automation\NPCWeatherAudit\WeatherManualRun' '-abslog=C:\URproject\drone\Saved\Automation\NPCWeatherAudit\WeatherManualRun.log'
```

AI에서는 시작 맵을 해당 TestMap으로 바꾸고 필터를 `Drone.AI.ShotgunSystemsTestMap` (Asset+PIE), 또는 `Drone.AI.NPCPerceptionSearchPIE+Drone.AI.PersonalWeaponEngagementPolicy+Drone.AI.NPCGreyboxAssets`로 바꾼다. 새 output 폴더를 사용해 과거 report와 혼동하지 않는다. Exit code뿐 아니라 `index.json`의 discovered test 수, failed/notRun과 개별 state를 확인한다.
