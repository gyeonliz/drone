#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/GameInstance.h"
#include "Flow/DroneGameFlowSubsystem.h"
#include "Mission/DroneMissionDefinition.h"

/**
 * TUT-PROGRESS-01: 튜토리얼 8개 수업이 NextMissionId로 이어져
 * - 순서·위치(n/8)를 계산하고,
 * - 성공 결과에서만 다음 수업 브리핑으로 넘어가며 클리어 시간·완료 기록이 남고,
 * - 마지막 수업 뒤에는 넘어갈 곳이 없고 8/8 완료가 되는지 확인한다.
 * 맵은 열지 않는다(상태 계약만). 화면 쪽은 Drone.Flow.TutorialNextLessonPIE가 본다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneTutorialProgressionTest,
	"Drone.Flow.TutorialProgression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace DroneTutorialProgression
{
const TArray<FName> ExpectedLessons = {
	FName(TEXT("Mission.Tutorial.Hover")),
	FName(TEXT("Mission.Tutorial.Forward")),
	FName(TEXT("Mission.Tutorial.Heading")),
	FName(TEXT("Mission.Tutorial.GateFlight")),
	FName(TEXT("Mission.Tutorial.FPV")),
	FName(TEXT("Mission.Tutorial.Payload")),
	FName(TEXT("Mission.Tutorial.UGV.NPC")),
	FName(TEXT("Mission.Tutorial.UGV.Turret"))};

/** 브리핑(MissionTrailer) 상태에서 출격까지 진행한다. */
bool LaunchFromBriefing(UDroneGameFlowSubsystem& Flow)
{
	return Flow.NotifyMissionTrailerFinished()
		&& Flow.NotifyMissionMapReady()
		&& !Flow.GetSnapshot().AvailableDroneIds.IsEmpty()
		&& Flow.SelectDrone(Flow.GetSnapshot().AvailableDroneIds[0])
		&& Flow.RequestMissionStart()
		&& Flow.ConsumeMissionStartRequest();
}
}

bool FDroneTutorialProgressionTest::RunTest(const FString& Parameters)
{
	using namespace DroneTutorialProgression;
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UDroneGameFlowSubsystem* Flow = NewObject<UDroneGameFlowSubsystem>(GameInstance);
	if (!TestTrue(TEXT("Catalog registers"), Flow && Flow->EnsureDefaultCatalog()))
	{
		return false;
	}

	// 순서
	TestTrue(TEXT("Hover starts the 8-lesson Tutorial chain in guide order"), Flow->GetMissionSequence(ExpectedLessons[0]) == ExpectedLessons);
	TestTrue(TEXT("Any lesson resolves the same chain"), Flow->GetMissionSequence(ExpectedLessons[4]) == ExpectedLessons);
	int32 Number = 0, Count = 0, Completed = 0;
	TestTrue(TEXT("Hover has a position"), Flow->GetMissionSequencePosition(ExpectedLessons[0], Number, Count, Completed));
	TestTrue(TEXT("Hover is lesson 1/8, none completed"), Number == 1 && Count == 8 && Completed == 0);
	TestTrue(TEXT("Turret is lesson 8/8"), Flow->GetMissionSequencePosition(ExpectedLessons[7], Number, Count, Completed) && Number == 8 && Count == 8);
	TestFalse(TEXT("Shared Training is not part of the chain"), Flow->GetMissionSequencePosition(TEXT("Mission.Tutorial.Training"), Number, Count, Completed));
	TestTrue(TEXT("Story M1 is 1/4 of its own chain, not the Tutorial one"), Flow->GetMissionSequencePosition(TEXT("Mission.Story.GoldenTime.Test"), Number, Count, Completed) && Number == 1 && Count == 4);
	TestEqual(TEXT("Unknown mission has no chain"), Flow->GetMissionSequence(TEXT("Mission.Unknown")).Num(), 0);

	// 고리로 잘못 이어도 멈춘다(메모리에서만 바꾸고 되돌림).
	if (UDroneMissionDefinition* Last = Flow->FindMissionDefinition(ExpectedLessons[7]))
	{
		const FName Original = Last->NextMissionId;
		Last->NextMissionId = ExpectedLessons[0];
		TestEqual(TEXT("A looped chain still terminates with 8 lessons"), Flow->GetMissionSequence(ExpectedLessons[3]).Num(), 8);
		Last->NextMissionId = Original;
	}

	// 실패하면 다음 수업으로 못 간다.
	TestTrue(TEXT("Opening → Lobby"), Flow->BeginOpeningTrailer() && Flow->EnterLobbyFromOpeningTrailer());
	TestTrue(TEXT("Hover selected and confirmed"), Flow->SelectMission(ExpectedLessons[0]) && Flow->ConfirmMissionSelection());
	TestTrue(TEXT("Hover launches"), LaunchFromBriefing(*Flow));
	TestTrue(TEXT("Hover fails"), Flow->CompleteMissionWithStoryFacts(EDroneMissionOutcome::Failure, {}, {}, 12.0));
	TestFalse(TEXT("Failure cannot advance to the next lesson"), Flow->RequestNextMission());
	TestFalse(TEXT("Failure does not mark the lesson complete"), Flow->IsMissionCompleted(ExpectedLessons[0]));
	TestTrue(TEXT("Failure still records elapsed time"), FMath::IsNearlyEqual(Flow->GetSnapshot().LastMissionElapsedSeconds, 12.0));
	TestTrue(TEXT("Retry reloads Hover"), Flow->RequestRetry() && Flow->NotifyMissionMapReady());
	TestTrue(TEXT("Retry clears the elapsed time"), Flow->GetSnapshot().LastMissionElapsedSeconds < 0.0);
	TestTrue(TEXT("Retry launches"), Flow->SelectDrone(Flow->GetSnapshot().AvailableDroneIds[0]) && Flow->RequestMissionStart() && Flow->ConsumeMissionStartRequest());

	// 8개를 [다음 수업]으로 끝까지 잇는다.
	for (int32 Index = 0; Index < ExpectedLessons.Num(); ++Index)
	{
		const FName Lesson = ExpectedLessons[Index];
		if (Index > 0 && !TestTrue(*FString::Printf(TEXT("%s launches from its briefing"), *Lesson.ToString()), LaunchFromBriefing(*Flow)))
		{
			return false;
		}
		TestEqual(TEXT("Current lesson is selected"), Flow->GetSnapshot().SelectedMissionId, Lesson);
		const double Seconds = 30.0 + Index;
		TestTrue(*FString::Printf(TEXT("%s succeeds"), *Lesson.ToString()),
			Flow->CompleteMissionWithStoryFacts(EDroneMissionOutcome::Success, {}, {}, Seconds));
		TestTrue(TEXT("Clear time is kept for the result screen"), FMath::IsNearlyEqual(Flow->GetSnapshot().LastMissionElapsedSeconds, Seconds));
		TestTrue(TEXT("Success marks the lesson complete"), Flow->IsMissionCompleted(Lesson));
		Flow->GetMissionSequencePosition(Lesson, Number, Count, Completed);
		TestEqual(*FString::Printf(TEXT("%d/8 completed after %s"), Index + 1, *Lesson.ToString()), Completed, Index + 1);

		if (Index + 1 < ExpectedLessons.Num())
		{
			TestEqual(TEXT("Next lesson is the following one"), Flow->GetNextMissionId(), ExpectedLessons[Index + 1]);
			TestTrue(TEXT("Next lesson request succeeds"), Flow->RequestNextMission());
			TestEqual(TEXT("Next lesson opens its briefing"), Flow->GetSnapshot().State, EDroneGameFlowState::MissionTrailer);
			TestEqual(TEXT("Next lesson becomes the selection"), Flow->GetSnapshot().SelectedMissionId, ExpectedLessons[Index + 1]);
			TestEqual(TEXT("Lobby remembers the next lesson"), Flow->GetLastLobbyMissionId(), ExpectedLessons[Index + 1]);
			TestTrue(TEXT("Next lesson starts with no elapsed time"), Flow->GetSnapshot().LastMissionElapsedSeconds < 0.0);
		}
		else
		{
			TestTrue(TEXT("Last lesson has no next"), Flow->GetNextMissionId().IsNone());
			TestFalse(TEXT("Last lesson cannot advance"), Flow->RequestNextMission());
			TestEqual(TEXT("All 8 lessons completed"), Completed, 8);
		}
	}

	// 전체 완료 화면 [시작 메뉴]: 결과 → 타이틀. 선택은 지우고 완료 기록은 남긴다.
	TestTrue(TEXT("[시작 메뉴] returns to the title"), Flow->RequestReturnToTitle());
	TestEqual(TEXT("Title state"), Flow->GetSnapshot().State, EDroneGameFlowState::OpeningTrailer);
	TestTrue(TEXT("Title clears the mission selection"), Flow->GetSnapshot().SelectedMissionId.IsNone());
	TestTrue(TEXT("Completion survives the title return"), Flow->IsMissionCompleted(ExpectedLessons[3]));
	TestFalse(TEXT("[시작 메뉴] only works from a result"), Flow->RequestReturnToTitle());

	// 로비 정렬: 튜토리얼은 수업 순서 뒤에 공용 Training, 스토리는 Figma 번호 M1 → M4.
	const TArray<FName> TutorialLobby = Flow->GetMissionIdsInLobbyOrder(EDroneMissionCategory::Tutorial);
	TestTrue(TEXT("Tutorial tab lists the 8 lessons first, in order"),
		TutorialLobby.Num() >= 9 && TArray<FName>(TutorialLobby.GetData(), 8) == ExpectedLessons);
	TestEqual(TEXT("Shared Training comes after the chain"), TutorialLobby.Num() >= 9 ? TutorialLobby[8] : NAME_None, FName(TEXT("Mission.Tutorial.Training")));
	const TArray<FName> StoryOrder = {
		FName(TEXT("Mission.Story.GoldenTime.Test")),
		FName(TEXT("Mission.Story.Intercept.Test")),
		FName(TEXT("Mission.Story.VeilBreaker.Test")),
		FName(TEXT("Mission.Story.Endgame.Test"))};
	TestTrue(TEXT("Story tab lists M1 → M4"), Flow->GetMissionIdsInLobbyOrder(EDroneMissionCategory::Mission) == StoryOrder);
	TestTrue(TEXT("Story missions form a 4-mission chain"), Flow->GetMissionSequence(StoryOrder[2]) == StoryOrder);

	// 결과 → 특정 미션 탭으로 로비 복귀([미션 진행]).
	TestTrue(TEXT("Title → Lobby"), Flow->EnterLobbyFromOpeningTrailer());
	TestTrue(TEXT("M1 selected and launched"), Flow->SelectMission(StoryOrder[0]) && Flow->ConfirmMissionSelection() && LaunchFromBriefing(*Flow));
	TestTrue(TEXT("M1 succeeds"), Flow->CompleteMissionWithStoryFacts(EDroneMissionOutcome::Success, {}, {}, 100.0));
	TestEqual(TEXT("M1 offers M2 next"), Flow->GetNextMissionId(), StoryOrder[1]);
	TestTrue(TEXT("Return to lobby focusing M1"), Flow->RequestReturnToLobbyFocusing(StoryOrder[0]) && Flow->ConsumeLobbyReturnRequest());
	TestEqual(TEXT("Lobby remembers the focused mission"), Flow->GetLastLobbyMissionId(), StoryOrder[0]);
	TestTrue(TEXT("Completion survives the lobby return"), Flow->IsMissionCompleted(ExpectedLessons[3]));
	return true;
}

#endif
