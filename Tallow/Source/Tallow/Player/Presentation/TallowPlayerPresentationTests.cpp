#include "Player/Presentation/TallowFirstPersonProjectionState.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTallowPlayerProjectionLifecycleTest,
	"Tallow.Presentation.Player.ProjectionLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTallowPlayerProjectionLifecycleTest::RunTest(const FString& Parameters)
{
	UCameraComponent* Camera = NewObject<UCameraComponent>();
	UStaticMeshComponent* Candle = NewObject<UStaticMeshComponent>();
	UStaticMeshComponent* Flame = NewObject<UStaticMeshComponent>();
	Camera->SetFieldOfView(70.f);
	Camera->SetFirstPersonFieldOfView(70.f);
	Camera->SetFirstPersonScale(.8f);
	Camera->SetEnableFirstPersonFieldOfView(false);
	Camera->SetEnableFirstPersonScale(false);
	Candle->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::None);
	Flame->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	{
		FTallowFirstPersonProjectionState Lease;
		Lease.Apply(Camera, Candle, Flame, 90.f, .6f);
		TestEqual(TEXT("World FOV untouched"), Camera->FieldOfView, 70.f);
		TestEqual(TEXT("Presentation FOV applied"), Camera->FirstPersonFieldOfView, 90.f);
		TestTrue(TEXT("Both camera projection switches enabled"), Camera->bEnableFirstPersonFieldOfView && Camera->bEnableFirstPersonScale);
		TestTrue(TEXT("Candle and flame share projection"), Candle->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::FirstPerson
			&& Flame->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::FirstPerson);
		Lease.Restore();
		TestEqual(TEXT("Prior FOV restored"), Camera->FirstPersonFieldOfView, 70.f);
		TestEqual(TEXT("Prior scale restored"), Camera->FirstPersonScale, .8f);
		TestFalse(TEXT("Prior disabled FOV restored"), !!Camera->bEnableFirstPersonFieldOfView);
		TestFalse(TEXT("Prior disabled scale restored"), !!Camera->bEnableFirstPersonScale);
		TestTrue(TEXT("Both distinct primitive states restored"), Candle->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::None
			&& Flame->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::WorldSpaceRepresentation);
		Lease.Restore();
		TestEqual(TEXT("Double disable is harmless"), Camera->FirstPersonFieldOfView, 70.f);
		Lease.Apply(Camera, Candle, Flame, 90.f, .6f);
	}
	TestEqual(TEXT("Scope teardown restores camera"), Camera->FirstPersonFieldOfView, 70.f);
	TestTrue(TEXT("Scope teardown restores prop"), Candle->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::None);
	{
		FTallowFirstPersonProjectionState Lease;
		Lease.Apply(Camera, Candle, Flame, 90.f, .6f);
		Camera->SetFirstPersonFieldOfView(100.f);
		Camera->SetFirstPersonScale(.4f);
		Candle->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
		Lease.Restore();
		TestEqual(TEXT("New caller FOV retained"), Camera->FirstPersonFieldOfView, 100.f);
		TestEqual(TEXT("New caller scale retained"), Camera->FirstPersonScale, .4f);
		TestTrue(TEXT("New caller FOV stays enabled"), !!Camera->bEnableFirstPersonFieldOfView);
		TestTrue(TEXT("New caller scale stays enabled"), !!Camera->bEnableFirstPersonScale);
		TestTrue(TEXT("New caller primitive state retained"), Candle->FirstPersonPrimitiveType == EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	}
	// FOV and scale are separate leases: changing one must not prevent restoration of the other.
	Camera->SetEnableFirstPersonFieldOfView(false);
	Camera->SetEnableFirstPersonScale(false);
	{
		FTallowFirstPersonProjectionState Lease;
		Lease.Apply(Camera, Candle, Flame, 90.f, .6f);
		Camera->SetFirstPersonFieldOfView(110.f);
		Lease.Restore();
		TestTrue(TEXT("Changed FOV retains its enabled state independently"), !!Camera->bEnableFirstPersonFieldOfView);
		TestFalse(TEXT("Unchanged scale restores its old disabled state"), !!Camera->bEnableFirstPersonScale);
		TestEqual(TEXT("Unchanged scale restores its old value"), Camera->FirstPersonScale, .4f);
	}
	return true;
}
#endif
