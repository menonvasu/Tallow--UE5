#include "Candle/Presentation/TallowFlameVisualComponent.h"
#include "Candle/Presentation/TallowFlameVisualState.h"
#include "Candle/TallowCandleFlameComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"

UTallowFlameVisualComponent::UTallowFlameVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTallowFlameVisualComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeVisual();
}

bool UTallowFlameVisualComponent::InitializeVisual()
{
	if (!IsValid(WickAnchor) || !GetOwner()) return false;
	if (!bStandalone)
	{
		if (!CandleSource) CandleSource = GetOwner()->FindComponentByClass<UTallowCandleComponent>();
		if (!GutterSource) GutterSource = GetOwner()->FindComponentByClass<UTallowCandleFlameComponent>();
	}
	if (!FlameMesh) FlameMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Presentation/Flame/SM_TallowFlame.SM_TallowFlame"));
	if (!FlameMaterial) FlameMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Presentation/Flame/M_TallowFlame.M_TallowFlame"));
	if (!FlameMesh || !FlameMaterial) return false;
	if (!Visual)
	{
		Visual = NewObject<UStaticMeshComponent>(GetOwner(), NAME_None, RF_Transient);
		Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Visual->SetGenerateOverlapEvents(false);
		Visual->SetCanEverAffectNavigation(false);
		Visual->SetCastShadow(false);
		if (bFirstPersonVisual) Visual->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
		Visual->SetVisibility(false);
		Visual->SetBoundsScale(100.f); // Unit source mesh; WPO height is clamped to 20 cm.
		Visual->RegisterComponent();
	}
	Visual->AttachToComponent(WickAnchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Visual->SetUsingAbsoluteScale(true);
	Visual->SetWorldScale3D(FVector::OneVector);
	Visual->SetStaticMesh(FlameMesh);
	Visual->SetEvaluateWorldPositionOffset(true);
	Material = UMaterialInstanceDynamic::Create(FlameMaterial, this);
	Visual->SetMaterial(0, Material);
	return Material != nullptr;
}

void UTallowFlameVisualComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Visual || !Material) return;
	const bool bHasSource = bStandalone || IsValid(CandleSource);
	const ECandleState State = bStandalone ? StandaloneState : (bHasSource ? CandleSource->GetCandleState() : ECandleState::Out);
	const bool bVisible = IsValid(WickAnchor) && bHasSource && State != ECandleState::Out
		&& (bStandalone || CandleSource->IsAcquired());
	Visual->SetVisibility(bVisible);
	if (bVisible && !bWasVisible) IgnitionRemaining = .25f;
	bWasVisible = bVisible;
	if (!bVisible)
	{
		IgnitionRemaining = 0.f;
		Exposure = 0.f;
		return;
	}
	const float Dt = FMath::Max(DeltaTime, 0.f);
	const auto* Holder = bStandalone ? nullptr : Cast<ATallowCharacter>(GetOwner());
	Calm = FMath::FInterpConstantTo(Calm, Holder && Holder->IsHoldingStill() ? 1.f : 0.f, Dt, Holder && Holder->IsHoldingStill() ? 2.5f : 4.f);
	// A held breath slows the flame's own flutter; the strain shakes it again (below).
	Elapsed += Dt * (1.f - .8f * Calm);
	float TargetExposure = StandaloneExposure;
	if (!bStandalone)
	{
		TargetExposure = CandleSource->IsGuttering() ? 1.f : (CandleSource->IsInDraft() ? .85f : 0.f);
		if (IsValid(GutterSource)) TargetExposure = FMath::Max(TargetExposure, GutterSource->GetGutterAmount());
	}
	Exposure = FMath::FInterpTo(Exposure, FMath::Clamp(TargetExposure * (1.f - .55f * Calm), 0.f, 1.f), Dt, 8.f);
	IgnitionRemaining = FMath::Max(0.f, IgnitionRemaining - Dt);
	const auto Shape = TallowFlameVisual::Evaluate(Elapsed + PhaseOffset, Exposure, State == ECandleState::Cupped, IgnitionRemaining / .25f);
	const float Height = FMath::Clamp(HeightCm, .1f, 20.f);
	Material->SetScalarParameterValue(TEXT("HeightCm"), Height * Shape.Height);
	Material->SetScalarParameterValue(TEXT("WidthCm"), FMath::Clamp(WidthCm, .05f, 10.f) * Shape.Width);
	Material->SetScalarParameterValue(TEXT("BendX"), Shape.BendX * Height);
	Material->SetScalarParameterValue(TEXT("BendY"), Shape.BendY * Height);
	Material->SetScalarParameterValue(TEXT("Emission"), 4.f * Shape.Emission);
}

void UTallowFlameVisualComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Visual) Visual->DestroyComponent();
	Visual = nullptr;
	Material = nullptr;
	Super::EndPlay(Reason);
}
