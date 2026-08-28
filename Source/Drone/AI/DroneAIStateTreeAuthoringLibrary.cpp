#include "AI/DroneAIStateTreeAuthoringLibrary.h"

#if WITH_EDITOR
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
	if (Root->Name != TEXT("HostilePatrol") || Root->Children.Num() != 4)
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
