#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Mission/DroneMissionDefinition.h"

/**
 * MISSION-CHECKPOINT-01: Figma에 정해진 미션별 실패 처리를 Data Asset이 그대로 갖고 있는지 확인한다.
 * - 재출격: 튜토리얼 8개(실패 → 스타트 지점에서 처음부터), M1 골든 타임, M3 베일 브레이커(UGV 재출격), M4 엔드게임(클리어까지 도전)
 * - 결과 화면: M2 인터셉트(실패 UI), 공용 종합 Training, Racing
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneFailureResponseDataTest,
	"Drone.Mission.FailureResponseData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneFailureResponseDataTest::RunTest(const FString& Parameters)
{
	const TArray<FString> Restart = {
		TEXT("Tutorial_Hover"), TEXT("Tutorial_Forward"), TEXT("Tutorial_Heading"), TEXT("Tutorial_GateFlight"),
		TEXT("Tutorial_Payload"), TEXT("Tutorial_FPV"), TEXT("Tutorial_UGV_NPC"), TEXT("Tutorial_UGV_Turret"),
		TEXT("Story_GoldenTime_Test"), TEXT("Story_VeilBreaker_Test"), TEXT("Story_Endgame_Test")};
	const TArray<FString> ShowResult = {
		TEXT("Story_Intercept_Test"), TEXT("Tutorial_Training"), TEXT("Racing_Circuit_Test")};

	auto Check = [this](const FString& Name, const EDroneMissionFailureResponse Expected)
	{
		const FString Path = FString::Printf(TEXT("/Game/Drone/Data/Missions/DA_Mission_%s.DA_Mission_%s"), *Name, *Name);
		const UDroneMissionDefinition* Mission = LoadObject<UDroneMissionDefinition>(nullptr, *Path);
		if (!TestNotNull(*FString::Printf(TEXT("%s loads"), *Name), Mission))
		{
			return;
		}
		TestEqual(*FString::Printf(TEXT("%s failure response"), *Name),
			static_cast<int32>(Mission->FailureResponse), static_cast<int32>(Expected));
		TestEqual(*FString::Printf(TEXT("%s restarts are unlimited"), *Name), Mission->MaxCheckpointRestarts, 0);
	};
	for (const FString& Name : Restart) Check(Name, EDroneMissionFailureResponse::RestartFromCheckpoint);
	for (const FString& Name : ShowResult) Check(Name, EDroneMissionFailureResponse::ShowResult);
	return true;
}

#endif
