#include "AI/Animation/DroneNPCAnimationAuthoringLibrary.h"

#if WITH_EDITOR
#include "AI/Animation/DroneNPCAnimInstance.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_ModifyBone.h"
#include "AnimGraphNode_Root.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimNodeBase.h"
#include "BlueprintEditorLibrary.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#endif

#if WITH_EDITOR
namespace
{
	const FName GazeLocalToComponentMarker(TEXT("DRONE_GAZE_LOCAL_TO_COMPONENT"));
	const FName GazeSpineMarker(TEXT("DRONE_GAZE_SPINE"));
	const FName GazeNeckMarker(TEXT("DRONE_GAZE_NECK"));
	const FName GazeHeadMarker(TEXT("DRONE_GAZE_HEAD"));
	const FName GazeComponentToLocalMarker(TEXT("DRONE_GAZE_COMPONENT_TO_LOCAL"));

	FString MakeObjectPath(const FString& AssetPath)
	{
		return FString::Printf(
			TEXT("%s.%s"),
			*AssetPath,
			*FPackageName::GetLongPackageAssetName(AssetPath));
	}

	bool IsPosePin(const UEdGraphPin* Pin)
	{
		if (!Pin || Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Struct)
		{
			return false;
		}

		const UObject* SubCategoryObject = Pin->PinType.PinSubCategoryObject.Get();
		return SubCategoryObject == FPoseLink::StaticStruct()
			|| SubCategoryObject == FComponentSpacePoseLink::StaticStruct();
	}

	UEdGraphPin* FindPosePin(UEdGraphNode* Node, const EEdGraphPinDirection Direction)
	{
		if (!Node)
		{
			return nullptr;
		}
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == Direction && IsPosePin(Pin))
			{
				return Pin;
			}
		}
		return nullptr;
	}

	UEdGraphNode* FindMarkedNode(const UAnimBlueprint* AnimBlueprint, const FName Marker)
	{
		if (!AnimBlueprint)
		{
			return nullptr;
		}

		TArray<UEdGraph*> Graphs;
		AnimBlueprint->GetAllGraphs(Graphs);
		for (UEdGraph* Graph : Graphs)
		{
			if (!Graph)
			{
				continue;
			}
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node && Node->NodeComment == Marker.ToString())
				{
					return Node;
				}
			}
		}
		return nullptr;
	}

	UAnimGraphNode_Root* FindRootNode(UAnimBlueprint* AnimBlueprint)
	{
		if (!AnimBlueprint)
		{
			return nullptr;
		}

		TArray<UEdGraph*> Graphs;
		AnimBlueprint->GetAllGraphs(Graphs);
		for (UEdGraph* Graph : Graphs)
		{
			if (!Graph)
			{
				continue;
			}
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (UAnimGraphNode_Root* RootNode = Cast<UAnimGraphNode_Root>(Node))
				{
					return RootNode;
				}
			}
		}
		return nullptr;
	}

	template <typename NodeType>
	NodeType* CreateMarkedNode(UEdGraph& Graph, const FName Marker, const int32 PosX, const int32 PosY)
	{
		FGraphNodeCreator<NodeType> Creator(Graph);
		NodeType* Node = Creator.CreateNode();
		Node->NodeComment = Marker.ToString();
		Node->NodePosX = PosX;
		Node->NodePosY = PosY;
		Creator.Finalize();
		return Node;
	}

	UK2Node_VariableGet* CreateVariableGetter(
		UEdGraph& Graph,
		const FName PropertyName,
		const int32 PosX,
		const int32 PosY)
	{
		FGraphNodeCreator<UK2Node_VariableGet> Creator(Graph);
		UK2Node_VariableGet* Node = Creator.CreateNode();
		Node->VariableReference.SetSelfMember(PropertyName);
		Node->NodePosX = PosX;
		Node->NodePosY = PosY;
		Creator.Finalize();
		return Node;
	}

	UAnimGraphNode_ModifyBone* CreateLookBoneNode(
		UEdGraph& Graph,
		const FName Marker,
		const FName BoneName,
		const FName RotationProperty,
		const int32 PosX,
		const int32 PosY)
	{
		FGraphNodeCreator<UAnimGraphNode_ModifyBone> Creator(Graph);
		UAnimGraphNode_ModifyBone* Node = Creator.CreateNode();
		Node->NodeComment = Marker.ToString();
		Node->NodePosX = PosX;
		Node->NodePosY = PosY;
		Node->Node.BoneToModify.BoneName = BoneName;
		Node->Node.RotationMode = BMM_Additive;
		// Manny Bone의 로컬 축은 화면상의 Yaw 축과 일치하지 않는다. Component Space를
		// 사용해야 Controller의 Pitch/Yaw가 실제 위아래/좌우 시선으로 그대로 적용된다.
		Node->Node.RotationSpace = BCS_ComponentSpace;
		Creator.Finalize();

		UK2Node_VariableGet* RotationGetter = CreateVariableGetter(
			Graph,
			RotationProperty,
			PosX - 260,
			PosY + 140);
		UK2Node_VariableGet* AlphaGetter = CreateVariableGetter(
			Graph,
			GET_MEMBER_NAME_CHECKED(UDroneNPCAnimInstance, DroneLookAlpha),
			PosX - 260,
			PosY + 220);

		UEdGraphPin* RotationPin = Node->FindPin(TEXT("Rotation"), EGPD_Input);
		UEdGraphPin* AlphaPin = Node->FindPin(TEXT("Alpha"), EGPD_Input);
		if (!RotationPin || !AlphaPin || !RotationGetter->GetValuePin() || !AlphaGetter->GetValuePin())
		{
			return nullptr;
		}
		RotationGetter->GetValuePin()->MakeLinkTo(RotationPin);
		AlphaGetter->GetValuePin()->MakeLinkTo(AlphaPin);
		return Node;
	}

	bool HasCompleteGazeChain(const UAnimBlueprint* AnimBlueprint)
	{
		UEdGraphNode* LocalToComponent = FindMarkedNode(AnimBlueprint, GazeLocalToComponentMarker);
		UEdGraphNode* Spine = FindMarkedNode(AnimBlueprint, GazeSpineMarker);
		UEdGraphNode* Neck = FindMarkedNode(AnimBlueprint, GazeNeckMarker);
		UEdGraphNode* Head = FindMarkedNode(AnimBlueprint, GazeHeadMarker);
		UEdGraphNode* ComponentToLocal = FindMarkedNode(AnimBlueprint, GazeComponentToLocalMarker);
		UEdGraphPin* LocalOutput = FindPosePin(LocalToComponent, EGPD_Output);
		UEdGraphPin* SpineInput = FindPosePin(Spine, EGPD_Input);
		UEdGraphPin* SpineOutput = FindPosePin(Spine, EGPD_Output);
		UEdGraphPin* NeckInput = FindPosePin(Neck, EGPD_Input);
		UEdGraphPin* NeckOutput = FindPosePin(Neck, EGPD_Output);
		UEdGraphPin* HeadInput = FindPosePin(Head, EGPD_Input);
		UEdGraphPin* HeadOutput = FindPosePin(Head, EGPD_Output);
		UEdGraphPin* ComponentInput = FindPosePin(ComponentToLocal, EGPD_Input);
		return LocalOutput && SpineInput && SpineOutput && NeckInput
			&& NeckOutput && HeadInput && HeadOutput && ComponentInput
			&& LocalOutput->LinkedTo.Contains(SpineInput)
			&& SpineOutput->LinkedTo.Contains(NeckInput)
			&& NeckOutput->LinkedTo.Contains(HeadInput)
			&& HeadOutput->LinkedTo.Contains(ComponentInput);
	}

	bool HasConfiguredGazeBoneNodes(const UAnimBlueprint* AnimBlueprint)
	{
		const FName Markers[] = {GazeSpineMarker, GazeNeckMarker, GazeHeadMarker};
		for (const FName Marker : Markers)
		{
			const UAnimGraphNode_ModifyBone* Node = Cast<UAnimGraphNode_ModifyBone>(
				FindMarkedNode(AnimBlueprint, Marker));
			if (!Node
				|| Node->Node.RotationMode != BMM_Additive
				|| Node->Node.RotationSpace != BCS_ComponentSpace)
			{
				return false;
			}
		}
		return true;
	}

	bool ConfigureGazeBoneNodes(UAnimBlueprint* AnimBlueprint)
	{
		const FName Markers[] = {GazeSpineMarker, GazeNeckMarker, GazeHeadMarker};
		bool bChanged = false;
		for (const FName Marker : Markers)
		{
			UAnimGraphNode_ModifyBone* Node = Cast<UAnimGraphNode_ModifyBone>(
				FindMarkedNode(AnimBlueprint, Marker));
			if (!Node)
			{
				return false;
			}

			if (Node->Node.RotationMode != BMM_Additive
				|| Node->Node.RotationSpace != BCS_ComponentSpace)
			{
				Node->Modify();
				Node->Node.RotationMode = BMM_Additive;
				Node->Node.RotationSpace = BCS_ComponentSpace;
				bChanged = true;
			}
		}

		if (bChanged)
		{
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
			FKismetEditorUtilities::CompileBlueprint(AnimBlueprint);
		}
		return HasConfiguredGazeBoneNodes(AnimBlueprint);
	}
}
#endif

bool UDroneNPCAnimationAuthoringLibrary::UpgradeRifleAnimBlueprintForDroneGaze(const FString& AssetPath)
{
#if WITH_EDITOR
	UAnimBlueprint* AnimBlueprint = LoadObject<UAnimBlueprint>(nullptr, *MakeObjectPath(AssetPath));
	if (!AnimBlueprint)
	{
		return false;
	}

	if (!AnimBlueprint->ParentClass->IsChildOf(UDroneNPCAnimInstance::StaticClass()))
	{
		UBlueprintEditorLibrary::ReparentBlueprint(AnimBlueprint, UDroneNPCAnimInstance::StaticClass());
		FKismetEditorUtilities::CompileBlueprint(AnimBlueprint);
	}

	if (HasCompleteGazeChain(AnimBlueprint))
	{
		return ConfigureGazeBoneNodes(AnimBlueprint)
			&& AnimBlueprint->Status != BS_Error;
	}

	// 중간까지만 생성된 Asset은 자동 덮어쓰지 않는다. 기존 Pose 연결을 잃지 않게 실패로 알린다.
	if (FindMarkedNode(AnimBlueprint, GazeLocalToComponentMarker)
		|| FindMarkedNode(AnimBlueprint, GazeSpineMarker)
		|| FindMarkedNode(AnimBlueprint, GazeNeckMarker)
		|| FindMarkedNode(AnimBlueprint, GazeHeadMarker)
		|| FindMarkedNode(AnimBlueprint, GazeComponentToLocalMarker))
	{
		return false;
	}

	UAnimGraphNode_Root* RootNode = FindRootNode(AnimBlueprint);
	UEdGraph* Graph = RootNode ? RootNode->GetGraph() : nullptr;
	UEdGraphPin* RootInput = FindPosePin(RootNode, EGPD_Input);
	if (!Graph || !RootInput || RootInput->LinkedTo.Num() != 1)
	{
		return false;
	}

	UEdGraphPin* ExistingPoseOutput = RootInput->LinkedTo[0];
	RootInput->BreakAllPinLinks();
	Graph->Modify();

	const int32 RootX = RootNode->NodePosX;
	const int32 RootY = RootNode->NodePosY;
	UAnimGraphNode_LocalToComponentSpace* LocalToComponent = CreateMarkedNode<UAnimGraphNode_LocalToComponentSpace>(
		*Graph, GazeLocalToComponentMarker, RootX - 1400, RootY);
	UAnimGraphNode_ModifyBone* Spine = CreateLookBoneNode(
		*Graph,
		GazeSpineMarker,
		TEXT("spine_03"),
		GET_MEMBER_NAME_CHECKED(UDroneNPCAnimInstance, DroneLookSpineRotation),
		RootX - 1100,
		RootY);
	UAnimGraphNode_ModifyBone* Neck = CreateLookBoneNode(
		*Graph,
		GazeNeckMarker,
		TEXT("neck_01"),
		GET_MEMBER_NAME_CHECKED(UDroneNPCAnimInstance, DroneLookNeckRotation),
		RootX - 800,
		RootY);
	UAnimGraphNode_ModifyBone* Head = CreateLookBoneNode(
		*Graph,
		GazeHeadMarker,
		TEXT("head"),
		GET_MEMBER_NAME_CHECKED(UDroneNPCAnimInstance, DroneLookHeadRotation),
		RootX - 500,
		RootY);
	UAnimGraphNode_ComponentToLocalSpace* ComponentToLocal = CreateMarkedNode<UAnimGraphNode_ComponentToLocalSpace>(
		*Graph, GazeComponentToLocalMarker, RootX - 200, RootY);

	if (!LocalToComponent || !Spine || !Neck || !Head || !ComponentToLocal)
	{
		return false;
	}

	UEdGraphPin* LocalInput = FindPosePin(LocalToComponent, EGPD_Input);
	UEdGraphPin* LocalOutput = FindPosePin(LocalToComponent, EGPD_Output);
	UEdGraphPin* SpineInput = FindPosePin(Spine, EGPD_Input);
	UEdGraphPin* SpineOutput = FindPosePin(Spine, EGPD_Output);
	UEdGraphPin* NeckInput = FindPosePin(Neck, EGPD_Input);
	UEdGraphPin* NeckOutput = FindPosePin(Neck, EGPD_Output);
	UEdGraphPin* HeadInput = FindPosePin(Head, EGPD_Input);
	UEdGraphPin* HeadOutput = FindPosePin(Head, EGPD_Output);
	UEdGraphPin* ComponentInput = FindPosePin(ComponentToLocal, EGPD_Input);
	UEdGraphPin* ComponentOutput = FindPosePin(ComponentToLocal, EGPD_Output);
	if (!ExistingPoseOutput || !LocalInput || !LocalOutput || !SpineInput || !SpineOutput
		|| !NeckInput || !NeckOutput || !HeadInput || !HeadOutput || !ComponentInput || !ComponentOutput)
	{
		return false;
	}

	ExistingPoseOutput->MakeLinkTo(LocalInput);
	LocalOutput->MakeLinkTo(SpineInput);
	SpineOutput->MakeLinkTo(NeckInput);
	NeckOutput->MakeLinkTo(HeadInput);
	HeadOutput->MakeLinkTo(ComponentInput);
	ComponentOutput->MakeLinkTo(RootInput);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	FKismetEditorUtilities::CompileBlueprint(AnimBlueprint);
	return AnimBlueprint->Status != BS_Error
		&& HasCompleteGazeChain(AnimBlueprint)
		&& HasConfiguredGazeBoneNodes(AnimBlueprint);
#else
	return false;
#endif
}

bool UDroneNPCAnimationAuthoringLibrary::ValidateRifleAnimBlueprintDroneGaze(const FString& AssetPath)
{
#if WITH_EDITOR
	const UAnimBlueprint* AnimBlueprint = LoadObject<UAnimBlueprint>(nullptr, *MakeObjectPath(AssetPath));
	return AnimBlueprint
		&& AnimBlueprint->ParentClass
		&& AnimBlueprint->ParentClass->IsChildOf(UDroneNPCAnimInstance::StaticClass())
		&& AnimBlueprint->Status != BS_Error
		&& HasCompleteGazeChain(AnimBlueprint)
		&& HasConfiguredGazeBoneNodes(AnimBlueprint);
#else
	return false;
#endif
}
