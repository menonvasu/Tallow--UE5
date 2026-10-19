#include "Candle/Presentation/TallowFlameVisualState.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTallowFlameAnchoringTest, "Tallow.Presentation.Flame.AnchorAndProtection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTallowFlameAnchoringTest::RunTest(const FString& Parameters)
{
	float ExposedBend = 0.f;
	float ProtectedBend = 0.f;
	float MinHeight = 10.f;
	float MaxHeight = 0.f;
	for (int32 Frame = 0; Frame < 600; ++Frame)
	{
		const float Time = Frame / 60.f;
		const auto Exposed = TallowFlameVisual::Evaluate(Time, 1.f, false, 0.f);
		const auto Protected = TallowFlameVisual::Evaluate(Time, 1.f, true, 0.f);
		const FVector Root = TallowFlameVisual::Deform(FVector::ZeroVector, 3.f, .9f, Exposed);
		if (!Root.Equals(FVector::ZeroVector, 0.f))
		{
			AddError(TEXT("Wick moved during deformation"));
			return false;
		}
		const FVector Tip = TallowFlameVisual::Deform(FVector(0, 0, 1), 3.f, .9f, Exposed);
		const FVector Mid = TallowFlameVisual::Deform(FVector(0, 0, .5), 3.f, .9f, Exposed);
		if (Tip.ContainsNaN() || Tip.Z <= 0.f || !FMath::IsNearlyEqual(Mid.X, Tip.X * .25, 1.e-5))
		{
			AddError(TEXT("Flame must remain finite, above wick, and bend non-rigidly"));
			return false;
		}
		ExposedBend += FMath::Abs(Exposed.BendX);
		ProtectedBend += FMath::Abs(Protected.BendX);
		MinHeight = FMath::Min(MinHeight, Exposed.Height);
		MaxHeight = FMath::Max(MaxHeight, Exposed.Height);
	}
	TestTrue(TEXT("Cupping reduces total deflection by at least 70 percent"), ProtectedBend < ExposedBend * .3f);
	TestTrue(TEXT("Strong draft visibly collapses and recovers"), MaxHeight - MinHeight > .3f);
	const auto Burning = TallowFlameVisual::Evaluate(1.f, 0.f, false, 0.f);
	const auto Igniting = TallowFlameVisual::Evaluate(1.f, 0.f, false, 1.f);
	TestTrue(TEXT("Successful relight produces short height flare"), Igniting.Height > Burning.Height);
	TestTrue(TEXT("Successful relight produces short emission flare"), Igniting.Emission > Burning.Emission);
	return true;
}
#endif
