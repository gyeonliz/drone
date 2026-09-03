#include "AI/DroneAIStateTreeAuthoringLibrary.h"

#if WITH_EDITOR
#include "AI/DroneAITags.h"
#include "AI/DroneNPCCoverStateTreeTasks.h"
#include "AI/DroneNPCMGTurretStateTreeTasks.h"
#include "AI/DroneNPCPerceptionStateTreeTasks.h"
#include "AI/DroneNPCPatrolStateTreeTasks.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StateTreeAIComponentSchema.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "StateTree.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditingSubsystem.h"
#include "StateTreeEditorData.h"
#include "StateTreeFactory.h"
#include "StateTreeState.h"
#include "UObject/Package.h"
#endif

#if WITH_EDITOR
namespace
{
	FString MakeStateTreeObjectPath(const FString& AssetPath)
	{
		return FString::Printf(
			TEXT("%s.%s"),
			*AssetPath,
			*FPackageName::GetLongPackageAssetName(AssetPath));
	}

	bool HasSingleTaskOfType(const UStateTreeState* State, const UScriptStruct* ExpectedType)
	{
		return State
			&& State->Tasks.Num() == 1
			&& State->Tasks[0].Node.GetScriptStruct() == ExpectedType;
	}

	bool HasEventTransitionTo(
		const UStateTreeState* State,
		const FGameplayTag EventTag,
		const FName TargetStateName)
	{
		return State && State->Transitions.ContainsByPredicate(
			[EventTag, TargetStateName](const FStateTreeTransition& Transition)
			{
				return Transition.Trigger == EStateTreeTransitionTrigger::OnEvent
					&& Transition.RequiredEvent.Tag == EventTag
					&& Transition.State.LinkType == EStateTreeTransitionType::GotoState
					&& Transition.State.Name == TargetStateName;
			});
	}

	void AddHostilePerceptionStates(UStateTreeState& Root, UStateTreeState& Claim)
	{
		UStateTreeState& Detected = Root.AddChildState(TEXT("DroneDetected"));
		UStateTreeState& Search = Root.AddChildState(TEXT("SearchLastKnownLocation"));

		Detected.AddTask<FDroneStateTreeDetectedTask>();
		Detected.AddTransition(
			EStateTreeTransitionTrigger::OnEvent,
			DroneAITags::Event_DroneLost,
			EStateTreeTransitionType::GotoState,
			&Search);

		Search.AddTask<FDroneStateTreeSearchTask>();
		Search.AddTransition(
			EStateTreeTransitionTrigger::OnEvent,
			DroneAITags::Event_DroneDetected,
			EStateTreeTransitionType::GotoState,
			&Detected);
		Search.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded, EStateTreeTransitionType::GotoState, &Claim);
		Search.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &Claim);

		// Root 전환은 Claim·Move·Wait·Release 어느 단계에서 감지해도 같은 대응 상태로 보낸다.
		Root.AddTransition(
			EStateTreeTransitionTrigger::OnEvent,
			DroneAITags::Event_DroneDetected,
			EStateTreeTransitionType::GotoState,
			&Detected);
	}

	void ReplaceEventTransition(
		UStateTreeState& State,
		const FGameplayTag EventTag,
		UStateTreeState& TargetState)
	{
		State.Transitions.RemoveAll(
			[EventTag](const FStateTreeTransition& Transition)
			{
				return Transition.Trigger == EStateTreeTransitionTrigger::OnEvent
					&& Transition.RequiredEvent.Tag == EventTag;
			});
		State.AddTransition(
			EStateTreeTransitionTrigger::OnEvent,
			EventTag,
			EStateTreeTransitionType::GotoState,
			&TargetState);
	}

	void AddHostileMGTurretStates(
		UStateTreeState& Root,
		UStateTreeState& Detected,
		UStateTreeState& Search)
	{
		UStateTreeState& ClaimMG = Root.AddChildState(TEXT("ClaimMGTurretSlot"));
		UStateTreeState& MoveMG = Root.AddChildState(TEXT("MoveToMGTurret"));
		UStateTreeState& HoldMG = Root.AddChildState(TEXT("HoldMGTurretReservation"));

		ClaimMG.AddTask<FDroneStateTreeClaimMGTurretTask>();
		ClaimMG.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded, EStateTreeTransitionType::GotoState, &MoveMG);
		ClaimMG.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &Detected);
		ClaimMG.AddTransition(
			EStateTreeTransitionTrigger::OnEvent,
			DroneAITags::Event_DroneLost,
			EStateTreeTransitionType::GotoState,
			&Search);

		MoveMG.AddTask<FDroneStateTreeMoveToMGTurretTask>();
		MoveMG.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded, EStateTreeTransitionType::GotoState, &HoldMG);
		MoveMG.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &Detected);
		MoveMG.AddTransition(
			EStateTreeTransitionTrigger::OnEvent,
			DroneAITags::Event_DroneLost,
			EStateTreeTransitionType::GotoState,
			&Search);

		HoldMG.AddTask<FDroneStateTreeHoldMGTurretTask>();
		HoldMG.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &Detected);
		HoldMG.AddTransition(
			EStateTreeTransitionTrigger::OnEvent,
			DroneAITags::Event_DroneLost,
			EStateTreeTransitionType::GotoState,
			&Search);

		// 감지 시 MG를 먼저 한 번 시도하고 권한 없음·점유 중·검색 실패면
		// 기존 DroneDetected 개인 무기 상태로 즉시 대체한다.
		ReplaceEventTransition(Root, DroneAITags::Event_DroneDetected, ClaimMG);
		ReplaceEventTransition(Search, DroneAITags::Event_DroneDetected, ClaimMG);
	}

	void AddHostileCoverStates(
		UStateTreeState& Root,
		UStateTreeState& Detected,
		UStateTreeState& Search,
		UStateTreeState& ClaimMG)
	{
		UStateTreeState& ClaimCover = Root.AddChildState(TEXT("ClaimCoverSlot"));
		UStateTreeState& MoveCover = Root.AddChildState(TEXT("MoveToCover"));
		UStateTreeState& UseCover = Root.AddChildState(TEXT("UseCover"));

		ClaimCover.AddTask<FDroneStateTreeClaimCoverTask>();
		ClaimCover.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded, EStateTreeTransitionType::GotoState, &MoveCover);
		ClaimCover.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &Detected);
		ClaimCover.AddTransition(EStateTreeTransitionTrigger::OnEvent, DroneAITags::Event_DroneLost, EStateTreeTransitionType::GotoState, &Search);

		MoveCover.AddTask<FDroneStateTreeMoveToCoverTask>();
		MoveCover.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded, EStateTreeTransitionType::GotoState, &UseCover);
		MoveCover.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &Detected);
		MoveCover.AddTransition(EStateTreeTransitionTrigger::OnEvent, DroneAITags::Event_DroneLost, EStateTreeTransitionType::GotoState, &Search);

		UseCover.AddTask<FDroneStateTreeUseCoverTask>();
		UseCover.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &Detected);
		UseCover.AddTransition(EStateTreeTransitionTrigger::OnEvent, DroneAITags::Event_DroneLost, EStateTreeTransitionType::GotoState, &Search);

		// MG 권한 없음·Slot 점유·검색 실패는 Cover를 먼저 시도하고, Cover도 없을 때만
		// 기존 DroneDetected 제자리 개인 무기 대응으로 내려간다.
		ClaimMG.Transitions.RemoveAll(
			[](const FStateTreeTransition& Transition)
			{
				return Transition.Trigger == EStateTreeTransitionTrigger::OnStateFailed;
			});
		ClaimMG.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &ClaimCover);
	}
}
#endif

bool UDroneAIStateTreeAuthoringLibrary::CreateHostilePatrolStateTree(const FString& AssetPath)
{
#if WITH_EDITOR
	if (!FPackageName::IsValidLongPackageName(AssetPath) || FindObject<UObject>(nullptr, *AssetPath)
		|| FPackageName::DoesPackageExist(AssetPath))
	{
		return false;
	}

	UPackage* Package = CreatePackage(*AssetPath);
	if (!Package)
	{
		return false;
	}

	const FName AssetName(*FPackageName::GetLongPackageAssetName(AssetPath));
	UStateTreeFactory* Factory = NewObject<UStateTreeFactory>();
	Factory->SetSchemaClass(UStateTreeAIComponentSchema::StaticClass());
	UStateTree* StateTree = Cast<UStateTree>(Factory->FactoryCreateNew(
		UStateTree::StaticClass(),
		Package,
		AssetName,
		RF_Public | RF_Standalone | RF_Transactional,
		nullptr,
		nullptr));
	if (!StateTree)
	{
		return false;
	}

	UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
	if (!EditorData || EditorData->SubTrees.Num() != 1 || !EditorData->SubTrees[0])
	{
		return false;
	}

	UStateTreeState& Root = *EditorData->SubTrees[0];
	Root.Name = TEXT("HostilePatrol");
	UStateTreeState& Claim = Root.AddChildState(TEXT("ClaimEnemyPatrolSlot"));
	UStateTreeState& Move = Root.AddChildState(TEXT("MoveToPatrolSlot"));
	UStateTreeState& Wait = Root.AddChildState(TEXT("WaitAtPatrolSlot"));
	UStateTreeState& Release = Root.AddChildState(TEXT("ReleasePatrolSlot"));

	Claim.AddTask<FDroneStateTreeClaimPatrolSlotTask>();
	Claim.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded, EStateTreeTransitionType::GotoState, &Move);

	Move.AddTask<FDroneStateTreeMoveToPatrolSlotTask>();
	Move.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded, EStateTreeTransitionType::GotoState, &Wait);
	Move.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &Release);

	Wait.AddTask<FDroneStateTreeWaitAtPatrolSlotTask>();
	Wait.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded, EStateTreeTransitionType::GotoState, &Release);
	Wait.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &Release);

	Release.AddTask<FDroneStateTreeReleasePatrolSlotTask>();
	Release.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded, EStateTreeTransitionType::GotoState, &Claim);
	AddHostilePerceptionStates(Root, Claim);
	AddHostileMGTurretStates(Root, *Root.Children[4], *Root.Children[5]);
	AddHostileCoverStates(Root, *Root.Children[4], *Root.Children[5], *Root.Children[6]);

	FStateTreeCompilerLog CompilerLog;
	if (!UStateTreeEditingSubsystem::CompileStateTree(StateTree, CompilerLog) || !StateTree->IsReadyToRun())
	{
		return false;
	}

	FAssetRegistryModule::AssetCreated(StateTree);
	StateTree->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

bool UDroneAIStateTreeAuthoringLibrary::ValidateHostilePatrolStateTree(const FString& AssetPath)
{
#if WITH_EDITOR
	if (!FPackageName::IsValidLongPackageName(AssetPath))
	{
		return false;
	}

	const UStateTree* StateTree = LoadObject<UStateTree>(nullptr, *MakeStateTreeObjectPath(AssetPath));
	const UStateTreeEditorData* EditorData = StateTree
		? Cast<UStateTreeEditorData>(StateTree->EditorData)
		: nullptr;
	if (!StateTree || !StateTree->IsReadyToRun() || !EditorData
		|| !EditorData->Schema || !EditorData->Schema->IsA<UStateTreeAIComponentSchema>()
		|| EditorData->SubTrees.Num() != 1 || !EditorData->SubTrees[0])
	{
		return false;
	}

	const UStateTreeState* Root = EditorData->SubTrees[0];
	if (Root->Name != TEXT("HostilePatrol") || Root->Children.Num() < 4)
	{
		return false;
	}

	const UStateTreeState* Claim = Root->Children[0];
	const UStateTreeState* Move = Root->Children[1];
	const UStateTreeState* Wait = Root->Children[2];
	const UStateTreeState* Release = Root->Children[3];
	return Claim && Claim->Name == TEXT("ClaimEnemyPatrolSlot")
		&& HasSingleTaskOfType(Claim, FDroneStateTreeClaimPatrolSlotTask::StaticStruct())
		&& Move && Move->Name == TEXT("MoveToPatrolSlot")
		&& HasSingleTaskOfType(Move, FDroneStateTreeMoveToPatrolSlotTask::StaticStruct())
		&& Wait && Wait->Name == TEXT("WaitAtPatrolSlot")
		&& HasSingleTaskOfType(Wait, FDroneStateTreeWaitAtPatrolSlotTask::StaticStruct())
		&& Release && Release->Name == TEXT("ReleasePatrolSlot")
		&& HasSingleTaskOfType(Release, FDroneStateTreeReleasePatrolSlotTask::StaticStruct());
#else
	return false;
#endif
}

bool UDroneAIStateTreeAuthoringLibrary::UpgradeHostilePatrolStateTreeForPerception(const FString& AssetPath)
{
#if WITH_EDITOR
	if (ValidateHostilePerceptionStateTree(AssetPath))
	{
		return true;
	}
	if (!ValidateHostilePatrolStateTree(AssetPath))
	{
		return false;
	}

	UStateTree* StateTree = LoadObject<UStateTree>(nullptr, *MakeStateTreeObjectPath(AssetPath));
	UStateTreeEditorData* EditorData = StateTree
		? Cast<UStateTreeEditorData>(StateTree->EditorData)
		: nullptr;
	if (!StateTree || !EditorData || EditorData->SubTrees.Num() != 1 || !EditorData->SubTrees[0])
	{
		return false;
	}

	UStateTreeState* Root = EditorData->SubTrees[0];
	if (Root->Children.Num() != 4 || !Root->Children[0])
	{
		// 알 수 없는 사용자 확장 Asset은 자동으로 덮어쓰지 않는다.
		return false;
	}

	StateTree->Modify();
	EditorData->Modify();
	Root->Modify();
	AddHostilePerceptionStates(*Root, *Root->Children[0]);

	FStateTreeCompilerLog CompilerLog;
	if (!UStateTreeEditingSubsystem::CompileStateTree(StateTree, CompilerLog) || !StateTree->IsReadyToRun())
	{
		return false;
	}

	StateTree->MarkPackageDirty();
	return ValidateHostilePerceptionStateTree(AssetPath);
#else
	return false;
#endif
}

bool UDroneAIStateTreeAuthoringLibrary::ValidateHostilePerceptionStateTree(const FString& AssetPath)
{
#if WITH_EDITOR
	if (!ValidateHostilePatrolStateTree(AssetPath))
	{
		return false;
	}

	const UStateTree* StateTree = LoadObject<UStateTree>(nullptr, *MakeStateTreeObjectPath(AssetPath));
	const UStateTreeEditorData* EditorData = StateTree
		? Cast<UStateTreeEditorData>(StateTree->EditorData)
		: nullptr;
	if (!EditorData || EditorData->SubTrees.Num() != 1 || !EditorData->SubTrees[0])
	{
		return false;
	}

	const UStateTreeState* Root = EditorData->SubTrees[0];
	if (Root->Children.Num() != 6 && Root->Children.Num() != 9 && Root->Children.Num() != 12)
	{
		return false;
	}

	const UStateTreeState* Detected = Root->Children[4];
	const UStateTreeState* Search = Root->Children[5];
	if (!Detected || !Search)
	{
		return false;
	}
	const FName DetectedEventTarget = Root->Children.Num() >= 9 && Root->Children[6]
		? Root->Children[6]->Name
		: Detected->Name;
	return Detected->Name == TEXT("DroneDetected")
		&& HasSingleTaskOfType(Detected, FDroneStateTreeDetectedTask::StaticStruct())
		&& Search && Search->Name == TEXT("SearchLastKnownLocation")
		&& HasSingleTaskOfType(Search, FDroneStateTreeSearchTask::StaticStruct())
		&& HasEventTransitionTo(Root, DroneAITags::Event_DroneDetected, DetectedEventTarget)
		&& HasEventTransitionTo(Detected, DroneAITags::Event_DroneLost, Search->Name)
		&& HasEventTransitionTo(Search, DroneAITags::Event_DroneDetected, DetectedEventTarget);
#else
	return false;
#endif
}

bool UDroneAIStateTreeAuthoringLibrary::UpgradeHostilePerceptionStateTreeForMGTurret(const FString& AssetPath)
{
#if WITH_EDITOR
	if (ValidateHostileMGTurretStateTree(AssetPath))
	{
		return true;
	}
	if (!ValidateHostilePerceptionStateTree(AssetPath))
	{
		return false;
	}

	UStateTree* StateTree = LoadObject<UStateTree>(nullptr, *MakeStateTreeObjectPath(AssetPath));
	UStateTreeEditorData* EditorData = StateTree
		? Cast<UStateTreeEditorData>(StateTree->EditorData)
		: nullptr;
	if (!StateTree || !EditorData || EditorData->SubTrees.Num() != 1 || !EditorData->SubTrees[0])
	{
		return false;
	}

	UStateTreeState* Root = EditorData->SubTrees[0];
	if (Root->Children.Num() != 6 || !Root->Children[4] || !Root->Children[5])
	{
		// 알 수 없는 사용자 확장 Asset은 자동으로 덮어쓰지 않는다.
		return false;
	}

	StateTree->Modify();
	EditorData->Modify();
	Root->Modify();
	AddHostileMGTurretStates(*Root, *Root->Children[4], *Root->Children[5]);

	FStateTreeCompilerLog CompilerLog;
	if (!UStateTreeEditingSubsystem::CompileStateTree(StateTree, CompilerLog) || !StateTree->IsReadyToRun())
	{
		return false;
	}

	StateTree->MarkPackageDirty();
	return ValidateHostileMGTurretStateTree(AssetPath);
#else
	return false;
#endif
}

bool UDroneAIStateTreeAuthoringLibrary::ValidateHostileMGTurretStateTree(const FString& AssetPath)
{
#if WITH_EDITOR
	if (!ValidateHostilePerceptionStateTree(AssetPath))
	{
		return false;
	}

	const UStateTree* StateTree = LoadObject<UStateTree>(nullptr, *MakeStateTreeObjectPath(AssetPath));
	const UStateTreeEditorData* EditorData = StateTree
		? Cast<UStateTreeEditorData>(StateTree->EditorData)
		: nullptr;
	if (!EditorData || EditorData->SubTrees.Num() != 1 || !EditorData->SubTrees[0])
	{
		return false;
	}

	const UStateTreeState* Root = EditorData->SubTrees[0];
	if (Root->Children.Num() != 9 && Root->Children.Num() != 12)
	{
		return false;
	}

	const UStateTreeState* Detected = Root->Children[4];
	const UStateTreeState* Search = Root->Children[5];
	const UStateTreeState* ClaimMG = Root->Children[6];
	const UStateTreeState* MoveMG = Root->Children[7];
	const UStateTreeState* HoldMG = Root->Children[8];
	return Detected && Search
		&& ClaimMG && ClaimMG->Name == TEXT("ClaimMGTurretSlot")
		&& HasSingleTaskOfType(ClaimMG, FDroneStateTreeClaimMGTurretTask::StaticStruct())
		&& MoveMG && MoveMG->Name == TEXT("MoveToMGTurret")
		&& HasSingleTaskOfType(MoveMG, FDroneStateTreeMoveToMGTurretTask::StaticStruct())
		&& HoldMG && HoldMG->Name == TEXT("HoldMGTurretReservation")
		&& HasSingleTaskOfType(HoldMG, FDroneStateTreeHoldMGTurretTask::StaticStruct())
		&& HasEventTransitionTo(Root, DroneAITags::Event_DroneDetected, ClaimMG->Name)
		&& HasEventTransitionTo(Search, DroneAITags::Event_DroneDetected, ClaimMG->Name)
		&& HasEventTransitionTo(ClaimMG, DroneAITags::Event_DroneLost, Search->Name)
		&& HasEventTransitionTo(MoveMG, DroneAITags::Event_DroneLost, Search->Name)
		&& HasEventTransitionTo(HoldMG, DroneAITags::Event_DroneLost, Search->Name);
#else
	return false;
#endif
}

bool UDroneAIStateTreeAuthoringLibrary::UpgradeHostileMGTurretStateTreeForCover(const FString& AssetPath)
{
#if WITH_EDITOR
	if (ValidateHostileCoverStateTree(AssetPath))
	{
		return true;
	}
	if (!ValidateHostileMGTurretStateTree(AssetPath))
	{
		return false;
	}

	UStateTree* StateTree = LoadObject<UStateTree>(nullptr, *MakeStateTreeObjectPath(AssetPath));
	UStateTreeEditorData* EditorData = StateTree ? Cast<UStateTreeEditorData>(StateTree->EditorData) : nullptr;
	if (!StateTree || !EditorData || EditorData->SubTrees.Num() != 1 || !EditorData->SubTrees[0])
	{
		return false;
	}

	UStateTreeState* Root = EditorData->SubTrees[0];
	if (Root->Children.Num() != 9 || !Root->Children[4] || !Root->Children[5] || !Root->Children[6])
	{
		return false;
	}

	StateTree->Modify();
	EditorData->Modify();
	Root->Modify();
	AddHostileCoverStates(*Root, *Root->Children[4], *Root->Children[5], *Root->Children[6]);

	FStateTreeCompilerLog CompilerLog;
	if (!UStateTreeEditingSubsystem::CompileStateTree(StateTree, CompilerLog) || !StateTree->IsReadyToRun())
	{
		return false;
	}
	StateTree->MarkPackageDirty();
	return ValidateHostileCoverStateTree(AssetPath);
#else
	return false;
#endif
}

bool UDroneAIStateTreeAuthoringLibrary::ValidateHostileCoverStateTree(const FString& AssetPath)
{
#if WITH_EDITOR
	if (!ValidateHostileMGTurretStateTree(AssetPath))
	{
		return false;
	}
	const UStateTree* StateTree = LoadObject<UStateTree>(nullptr, *MakeStateTreeObjectPath(AssetPath));
	const UStateTreeEditorData* EditorData = StateTree ? Cast<UStateTreeEditorData>(StateTree->EditorData) : nullptr;
	if (!EditorData || EditorData->SubTrees.Num() != 1 || !EditorData->SubTrees[0])
	{
		return false;
	}
	const UStateTreeState* Root = EditorData->SubTrees[0];
	if (Root->Children.Num() != 12)
	{
		return false;
	}

	const UStateTreeState* Detected = Root->Children[4];
	const UStateTreeState* Search = Root->Children[5];
	const UStateTreeState* ClaimMG = Root->Children[6];
	const UStateTreeState* ClaimCover = Root->Children[9];
	const UStateTreeState* MoveCover = Root->Children[10];
	const UStateTreeState* UseCover = Root->Children[11];
	return Detected && Search && ClaimMG
		&& ClaimCover && ClaimCover->Name == TEXT("ClaimCoverSlot")
		&& HasSingleTaskOfType(ClaimCover, FDroneStateTreeClaimCoverTask::StaticStruct())
		&& MoveCover && MoveCover->Name == TEXT("MoveToCover")
		&& HasSingleTaskOfType(MoveCover, FDroneStateTreeMoveToCoverTask::StaticStruct())
		&& UseCover && UseCover->Name == TEXT("UseCover")
		&& HasSingleTaskOfType(UseCover, FDroneStateTreeUseCoverTask::StaticStruct())
		&& ClaimMG->Transitions.ContainsByPredicate(
			[ClaimCover](const FStateTreeTransition& Transition)
			{
				return Transition.Trigger == EStateTreeTransitionTrigger::OnStateFailed
					&& Transition.State.LinkType == EStateTreeTransitionType::GotoState
					&& Transition.State.Name == ClaimCover->Name;
			})
		&& HasEventTransitionTo(ClaimCover, DroneAITags::Event_DroneLost, Search->Name)
		&& HasEventTransitionTo(MoveCover, DroneAITags::Event_DroneLost, Search->Name)
		&& HasEventTransitionTo(UseCover, DroneAITags::Event_DroneLost, Search->Name);
#else
	return false;
#endif
}

bool UDroneAIStateTreeAuthoringLibrary::CreateFriendlyBaseRoutineStateTree(const FString& AssetPath)
{
#if WITH_EDITOR
	if (!FPackageName::IsValidLongPackageName(AssetPath) || FindObject<UObject>(nullptr, *AssetPath)
		|| FPackageName::DoesPackageExist(AssetPath))
	{
		return false;
	}

	UPackage* Package = CreatePackage(*AssetPath);
	if (!Package)
	{
		return false;
	}

	const FName AssetName(*FPackageName::GetLongPackageAssetName(AssetPath));
	UStateTreeFactory* Factory = NewObject<UStateTreeFactory>();
	Factory->SetSchemaClass(UStateTreeAIComponentSchema::StaticClass());
	UStateTree* StateTree = Cast<UStateTree>(Factory->FactoryCreateNew(
		UStateTree::StaticClass(),
		Package,
		AssetName,
		RF_Public | RF_Standalone | RF_Transactional,
		nullptr,
		nullptr));
	if (!StateTree)
	{
		return false;
	}

	UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
	if (!EditorData || EditorData->SubTrees.Num() != 1 || !EditorData->SubTrees[0])
	{
		return false;
	}

	UStateTreeState& Root = *EditorData->SubTrees[0];
	Root.Name = TEXT("FriendlyBaseRoutine");
	UStateTreeState& Claim = Root.AddChildState(TEXT("ClaimFriendlyActivitySlot"));
	UStateTreeState& Move = Root.AddChildState(TEXT("MoveToFriendlyActivitySlot"));
	UStateTreeState& Wait = Root.AddChildState(TEXT("WaitAtFriendlyActivitySlot"));
	UStateTreeState& Release = Root.AddChildState(TEXT("ReleaseFriendlyActivitySlot"));

	Claim.AddTask<FDroneStateTreeClaimFriendlyActivityTask>();
	Claim.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded, EStateTreeTransitionType::GotoState, &Move);

	// 이동·대기 Task는 예약된 Slot만 사용하므로 Hostile/Friendly가 안전하게 공유한다.
	Move.AddTask<FDroneStateTreeMoveToPatrolSlotTask>();
	Move.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded, EStateTreeTransitionType::GotoState, &Wait);
	Move.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &Release);

	Wait.AddTask<FDroneStateTreeWaitAtPatrolSlotTask>();
	Wait.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded, EStateTreeTransitionType::GotoState, &Release);
	Wait.AddTransition(EStateTreeTransitionTrigger::OnStateFailed, EStateTreeTransitionType::GotoState, &Release);

	Release.AddTask<FDroneStateTreeReleaseFriendlyActivityTask>();
	Release.AddTransition(EStateTreeTransitionTrigger::OnStateSucceeded, EStateTreeTransitionType::GotoState, &Claim);

	FStateTreeCompilerLog CompilerLog;
	if (!UStateTreeEditingSubsystem::CompileStateTree(StateTree, CompilerLog) || !StateTree->IsReadyToRun())
	{
		return false;
	}

	FAssetRegistryModule::AssetCreated(StateTree);
	StateTree->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

bool UDroneAIStateTreeAuthoringLibrary::ValidateFriendlyBaseRoutineStateTree(const FString& AssetPath)
{
#if WITH_EDITOR
	if (!FPackageName::IsValidLongPackageName(AssetPath))
	{
		return false;
	}

	const UStateTree* StateTree = LoadObject<UStateTree>(nullptr, *MakeStateTreeObjectPath(AssetPath));
	const UStateTreeEditorData* EditorData = StateTree
		? Cast<UStateTreeEditorData>(StateTree->EditorData)
		: nullptr;
	if (!StateTree || !StateTree->IsReadyToRun() || !EditorData
		|| !EditorData->Schema || !EditorData->Schema->IsA<UStateTreeAIComponentSchema>()
		|| EditorData->SubTrees.Num() != 1 || !EditorData->SubTrees[0])
	{
		return false;
	}

	const UStateTreeState* Root = EditorData->SubTrees[0];
	if (Root->Name != TEXT("FriendlyBaseRoutine") || Root->Children.Num() != 4)
	{
		return false;
	}

	const UStateTreeState* Claim = Root->Children[0];
	const UStateTreeState* Move = Root->Children[1];
	const UStateTreeState* Wait = Root->Children[2];
	const UStateTreeState* Release = Root->Children[3];
	return Claim && Claim->Name == TEXT("ClaimFriendlyActivitySlot")
		&& HasSingleTaskOfType(Claim, FDroneStateTreeClaimFriendlyActivityTask::StaticStruct())
		&& Move && Move->Name == TEXT("MoveToFriendlyActivitySlot")
		&& HasSingleTaskOfType(Move, FDroneStateTreeMoveToPatrolSlotTask::StaticStruct())
		&& Wait && Wait->Name == TEXT("WaitAtFriendlyActivitySlot")
		&& HasSingleTaskOfType(Wait, FDroneStateTreeWaitAtPatrolSlotTask::StaticStruct())
		&& Release && Release->Name == TEXT("ReleaseFriendlyActivitySlot")
		&& HasSingleTaskOfType(Release, FDroneStateTreeReleaseFriendlyActivityTask::StaticStruct());
#else
	return false;
#endif
}
