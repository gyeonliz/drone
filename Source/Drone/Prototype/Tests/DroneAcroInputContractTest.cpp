#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"

#include "EnhancedActionKeyMapping.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

namespace DroneAcroInputContractTest
{
constexpr const TCHAR* MappingContextPath =
	TEXT("/Game/Drone/Prototype/Input/IMC_DronePrototype.IMC_DronePrototype");

struct FExpectedAction
{
	const TCHAR* Path;
	TArray<FKey> PositiveKeys;
	TArray<FKey> NegativeKeys;
};

bool HasNegateModifier(const FEnhancedActionKeyMapping& Mapping)
{
	return Mapping.Modifiers.ContainsByPredicate([](const TObjectPtr<UInputModifier>& Modifier)
	{
		return Modifier && Modifier->IsA<UInputModifierNegate>();
	});
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDroneAcroInputContractTest,
	"Drone.Prototype.AcroInputContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDroneAcroInputContractTest::RunTest(const FString& Parameters)
{
	using namespace DroneAcroInputContractTest;

	UInputMappingContext* MappingContext = LoadObject<UInputMappingContext>(nullptr, MappingContextPath);
	TestNotNull(TEXT("Prototype Input Mapping Context exists"), MappingContext);
	if (!MappingContext)
	{
		return false;
	}

	const TArray<FExpectedAction> ExpectedActions{
		{
			TEXT("/Game/Drone/Prototype/Input/Actions/IA_DronePrototype_AcroPitch.IA_DronePrototype_AcroPitch"),
			{EKeys::W, EKeys::Gamepad_RightY},
			{EKeys::S}
		},
		{
			TEXT("/Game/Drone/Prototype/Input/Actions/IA_DronePrototype_AcroRoll.IA_DronePrototype_AcroRoll"),
			{EKeys::D, EKeys::Gamepad_RightX},
			{EKeys::A}
		},
		{
			TEXT("/Game/Drone/Prototype/Input/Actions/IA_DronePrototype_AcroYaw.IA_DronePrototype_AcroYaw"),
			{EKeys::E, EKeys::Gamepad_LeftX},
			{EKeys::Q}
		},
		{
			TEXT("/Game/Drone/Prototype/Input/Actions/IA_DronePrototype_AcroThrottle.IA_DronePrototype_AcroThrottle"),
			{EKeys::SpaceBar, EKeys::Gamepad_LeftY},
			{EKeys::LeftControl}
		}
	};

	const TArray<FEnhancedActionKeyMapping>& Mappings = MappingContext->GetMappings();
	for (const FExpectedAction& Expected : ExpectedActions)
	{
		UInputAction* Action = LoadObject<UInputAction>(nullptr, Expected.Path);
		TestNotNull(*FString::Printf(TEXT("Acro action exists: %s"), Expected.Path), Action);
		if (!Action)
		{
			continue;
		}

		TestEqual(
			*FString::Printf(TEXT("%s is an Axis1D action"), *Action->GetName()),
			Action->ValueType,
			EInputActionValueType::Axis1D);

		for (const FKey& Key : Expected.PositiveKeys)
		{
			const FEnhancedActionKeyMapping* Mapping = Mappings.FindByPredicate(
				[Action, Key](const FEnhancedActionKeyMapping& Candidate)
				{
					return Candidate.Action == Action && Candidate.Key == Key;
				});
			TestNotNull(
				*FString::Printf(TEXT("%s has positive mapping %s"), *Action->GetName(), *Key.ToString()),
				Mapping);
			if (Mapping)
			{
				TestFalse(TEXT("Positive mapping is not negated"), HasNegateModifier(*Mapping));
			}
		}

		for (const FKey& Key : Expected.NegativeKeys)
		{
			const FEnhancedActionKeyMapping* Mapping = Mappings.FindByPredicate(
				[Action, Key](const FEnhancedActionKeyMapping& Candidate)
				{
					return Candidate.Action == Action && Candidate.Key == Key;
				});
			TestNotNull(
				*FString::Printf(TEXT("%s has negative mapping %s"), *Action->GetName(), *Key.ToString()),
				Mapping);
			if (Mapping)
			{
				TestTrue(TEXT("Negative mapping has a Negate modifier"), HasNegateModifier(*Mapping));
			}
		}
	}

	return !HasAnyErrors();
}

#endif
