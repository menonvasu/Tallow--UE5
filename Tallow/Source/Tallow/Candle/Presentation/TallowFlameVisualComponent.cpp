#include "Candle/Presentation/TallowFlameVisualComponent.h"
#include "Player/TallowCharacter.h"
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
		if (CandleSource) CandleSource->OnCandleStateChanged.AddUniqueDynamic(this, &UTallowFlameVisualComponent::OnCandleState);
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
	if (bVisible != bWasVisible) LogVisibility(bVisible);
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
	// Mim close (29 Oct: your candle betrays you): how hard she pushes the flame, which way (wick space), and the gust.
	float Pushed = 0.f, GustNow = 0.f; FVector2D Away = FVector2D::ZeroVector;
	if (!bStandalone)
	{
		TargetExposure = CandleSource->IsGuttering() ? 1.f : (CandleSource->IsInDraft() ? .85f : 0.f);
		TargetExposure=FMath::Max(TargetExposure,FMath::Clamp((.2f-CandleSource->GetWaxFraction())/.2f,0.f,1.f)*.92f);
		if (IsValid(GutterSource))
		{
			TargetExposure = FMath::Max(TargetExposure, GutterSource->GetGutterAmount());
			const FVector Lean = GutterSource->GetGutterLean();
			Pushed = FMath::Clamp(static_cast<float>(Lean.Size()), 0.f, 1.f);
			GustNow = GutterSource->GetGustAmount();
			TargetExposure = FMath::Max(TargetExposure, GustNow);
			const FVector Local = WickAnchor->GetComponentTransform().InverseTransformVectorNoScale(Lean.GetSafeNormal());
			Away = FVector2D(Local.X, Local.Y).GetSafeNormal();
		}
	}
	Exposure = FMath::FInterpTo(Exposure, FMath::Clamp(TargetExposure * (1.f - .55f * Calm), 0.f, 1.f), Dt, 8.f);
	IgnitionRemaining = FMath::Max(0.f, IgnitionRemaining - Dt);
	auto Shape = TallowFlameVisual::Evaluate(Elapsed + PhaseOffset, Exposure, State == ECandleState::Cupped, IgnitionRemaining / .25f);
	Shape.BendX *= 1.f - .75f * Calm; Shape.BendY *= 1.f - .75f * Calm;
	if (!Away.IsNearlyZero())
	{
		// Her guttering pushes the flame away from her, the way she is coming, and it shakes (Nell: "the flame shakes
		// when it is close"); a held breath steadies the shaking, not the push. The gust lays it flat the same way just
		// before it goes. A cupped hand keeps most of it off.
		const FVector2D Bend(Shape.BendX, Shape.BendY), Side(-Away.Y, Away.X);
		const float Now = GetWorld()->GetTimeSeconds();
		const float Shield = State == ECandleState::Cupped ? .3f : 1.f;
		const FVector2D Shake = FVector2D(FMath::PerlinNoise1D(Now * 23.f + 11.f), FMath::PerlinNoise1D(Now * 19.f + 5.f))
			* .18f * Pushed * Pushed * (1.f - .5f * Calm);
		const FVector2D Bent = FMath::Lerp(Bend, Away * Bend.X + Side * Bend.Y, FMath::Clamp(Pushed * 4.f, 0.f, 1.f))
			+ (Away * (.22f * Pushed + 1.1f * GustNow) + Shake) * Shield;
		Shape.BendX = Bent.X; Shape.BendY = Bent.Y;
		Shape.Height *= 1.f - .5f * GustNow * Shield;
	}
	const float WaxStrength=bStandalone ? 1.f : FMath::Lerp(.48f,1.f,FMath::Clamp(CandleSource->GetWaxFraction()/.35f,0.f,1.f));
	const float Height = FMath::Clamp(HeightCm, .1f, 20.f)*WaxStrength;
	// Seen (25 Oct: you could not tell): at its call the flame bows hard toward the Keeper and shrinks, then stays
	// restless while it keeps looking. Cosmetic only; the light's reach for the Keeper is unchanged.
	float SeenHeight = 1.f, SeenEmission = 1.f; FVector2D SeenBend = FVector2D::ZeroVector;
	if (const auto* Seer = Cast<ATallowCharacter>(GetOwner()); Seer && !bStandalone)
	{
		const float Call = FMath::Exp(-FMath::Max(0.f, GetWorld()->GetTimeSeconds() - Seer->SeenCallAt) / .3f);
		const float Watched = FMath::Clamp(Seer->KeeperSight, 0.f, 1.f);
		const FVector Toward = WickAnchor->GetComponentTransform().InverseTransformVectorNoScale(
			(Seer->KeeperSeenFrom - WickAnchor->GetComponentLocation()).GetSafeNormal());
		const float Restless = Watched * (.35f + .25f * FMath::PerlinNoise1D(Elapsed * 9.f));
		SeenBend = FVector2D(Toward.X, Toward.Y).GetSafeNormal() * (.9f * Call + Restless);
		// The draught: a slow lean toward where the house wants her next (Player->GuideTarget, the next corner of her
		// walk to it), drawn in long soft pulls like air moving through the house. The sudden, hard, cold bow above is
		// being seen; the two must never read alike (28 Oct), so the draught swells and eases every few seconds and
		// never goes blue or small. 29 Oct: readable at a glance, and harder the closer she is: a plain lean across
		// the house, a strong one from about 2.5 m out. Her cupped hand keeps most of it off.
		if (!Seer->GuideTarget.IsZero() && Watched < .1f && Call < .05f
			&& FVector::Dist2D(Seer->GuideTarget, WickAnchor->GetComponentLocation()) > 40.f)
		{
			const FVector Way = WickAnchor->GetComponentTransform().InverseTransformVectorNoScale(
				(Seer->GuideTarget - WickAnchor->GetComponentLocation()).GetSafeNormal2D());
			const float Pull = FMath::Square(.5f + .5f * FMath::Sin(GetWorld()->GetTimeSeconds() * 1.7f));
			const float Near = 1.f - FMath::Clamp((Seer->GuideWalk - 250.f) / 2250.f, 0.f, 1.f);
			// With Mim close the warning wins: the draught gives way to her push.
			const float Lean = (.18f + .22f * Near + (.07f + .05f * Near) * Pull) * (1.f + .6f * FMath::Clamp(Seer->GuideEmphasis, 0.f, 1.f))
				* (1.f - .6f * Calm) * (State == ECandleState::Cupped ? .35f : 1.f) * (1.f - FMath::Clamp(Pushed * 1.5f, 0.f, 1.f));
			DraughtBend = FMath::Vector2DInterpTo(DraughtBend, FVector2D(Way.X, Way.Y).GetSafeNormal() * Lean, GetWorld()->GetDeltaSeconds(), 1.6f);
		}
		else DraughtBend = FMath::Vector2DInterpTo(DraughtBend, FVector2D::ZeroVector, GetWorld()->GetDeltaSeconds(), 3.f);
		SeenBend += DraughtBend;
		// Holding her breath too long: the flame trembles with her hand.
		const float Strain = Seer->GetHeldStrain();
		const float Now = GetWorld()->GetTimeSeconds();
		SeenBend += FVector2D(FMath::PerlinNoise1D(Now * 31.f), FMath::PerlinNoise1D(Now * 27.f + 3.f)) * .14f * Strain;
		SeenHeight = 1.f - .6f * Call - .2f * Watched;
		SeenEmission = 1.f - .3f * FMath::Max(Call, Watched);
	}
	Material->SetScalarParameterValue(TEXT("HeightCm"), Height * Shape.Height * SeenHeight);
	Material->SetScalarParameterValue(TEXT("WidthCm"), FMath::Clamp(WidthCm, .05f, 10.f) * Shape.Width);
	Material->SetScalarParameterValue(TEXT("BendX"), (Shape.BendX + SeenBend.X) * Height);
	Material->SetScalarParameterValue(TEXT("BendY"), (Shape.BendY + SeenBend.Y) * Height);
	Material->SetScalarParameterValue(TEXT("Emission"), 4.f * Shape.Emission * SeenEmission);
}

void UTallowFlameVisualComponent::OnCandleState(ECandleState NewState)
{
	if (!Visual || !Material || bStandalone || !IsValid(CandleSource)) return;
	const bool bVisible = IsValid(WickAnchor) && NewState != ECandleState::Out && CandleSource->IsAcquired();
	if (bVisible == bWasVisible) return;
	Visual->SetVisibility(bVisible);
	LogVisibility(bVisible);
	bWasVisible = bVisible;
	if (!bVisible) { IgnitionRemaining = 0.f; Exposure = 0.f; return; }
	// The first frame of the catch, as the next tick would draw it.
	IgnitionRemaining = .25f;
	const auto Shape = TallowFlameVisual::Evaluate(Elapsed + PhaseOffset, Exposure, NewState == ECandleState::Cupped, 1.f);
	const float Height = FMath::Clamp(HeightCm, .1f, 20.f) * FMath::Lerp(.48f, 1.f, FMath::Clamp(CandleSource->GetWaxFraction() / .35f, 0.f, 1.f));
	Material->SetScalarParameterValue(TEXT("HeightCm"), Height * Shape.Height);
	Material->SetScalarParameterValue(TEXT("WidthCm"), FMath::Clamp(WidthCm, .05f, 10.f) * Shape.Width);
	Material->SetScalarParameterValue(TEXT("BendX"), Shape.BendX * Height);
	Material->SetScalarParameterValue(TEXT("BendY"), Shape.BendY * Height);
	Material->SetScalarParameterValue(TEXT("Emission"), 4.f * Shape.Emission);
}

void UTallowFlameVisualComponent::LogVisibility(bool bVisible) const
{
	UE_LOG(LogTemp, Display, TEXT("TALLOW_CANDLE flame %s %s f=%llu t=%.3f"), bVisible ? TEXT("on") : TEXT("off"), *GetName(),
		(unsigned long long)GFrameCounter, GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f);
}

void UTallowFlameVisualComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Visual) Visual->DestroyComponent();
	Visual = nullptr;
	Material = nullptr;
	Super::EndPlay(Reason);
}
