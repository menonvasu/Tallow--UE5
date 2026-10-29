#include "Candle/TallowCandleFlameComponent.h"
#include "Candle/TallowCandleComponent.h"
#include "AI/TallowCreature.h"
#include "Components/PointLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMesh.h"
#include "UI/TallowControlsWidget.h"
#include "Player/TallowCharacter.h"
#include "GameMode/TallowGameMode.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Noise/TallowNoiseSubsystem.h"
#include "AI/Hunter/TallowHunterDirector.h"
#include "AI/Hunter/TallowHunterAIController.h"
#include "Chapter/TallowChapterSubsystem.h"
#include "Scare/TallowScareDirector.h"

namespace
{
FVector VisibleCandleLightPosition(AActor* Holder, const UCameraComponent* Camera, const FVector& WickPosition, float HoldDistance)
{
	const FTransform View = Camera->GetComponentTransform();
	FVector InView = View.InverseTransformPosition(WickPosition);
	// First-person hand poses can take the physical wick into a ledge, tabletop
	// or wall even while its separately projected mesh still looks visible.
	InView.X = FMath::Clamp(InView.X, 8.f, HoldDistance > 0.f ? FMath::Max(8.f, HoldDistance) : 55.f);
	InView.Y = FMath::Clamp(InView.Y, -30.f, 30.f);
	InView.Z = FMath::Clamp(InView.Z, -22.f, 20.f);
	const FVector Eye = View.GetLocation();
	const FVector Desired = View.TransformPosition(InView);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(CandleLightClearance), false, Holder);
	TArray<AActor*> Attached; Holder->GetAttachedActors(Attached); Query.AddIgnoredActors(Attached);
	FHitResult Hit;
	if (Holder->GetWorld()->SweepSingleByChannel(Hit, Eye, Desired, FQuat::Identity,
		ECC_Visibility, FCollisionShape::MakeSphere(5.f), Query))
	{
		if (Hit.bStartPenetrating) return Eye;
		// Keep the emitter on the viewing side of the obstruction, with a gap
		// large enough for the flame's existing positional flicker.
		return Hit.Location + Hit.Normal * 6.f;
	}
	return Desired;
}

bool IsStandingAboveRoomFloor(const ATallowCharacter* Holder)
{
	const auto* Capsule = Holder->GetCapsuleComponent();
	const FVector Feet = Capsule->GetComponentLocation() - FVector::UpVector * Capsule->GetScaledCapsuleHalfHeight();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(CandleRaisedSupport), false, Holder);
	FHitResult Support, Below;
	if (!Holder->GetWorld()->LineTraceSingleByChannel(Support, Feet + FVector::UpVector * 10.f,
		Feet - FVector::UpVector * 35.f, ECC_Visibility, Query) || Support.ImpactNormal.Z < .7f) return false;
	auto IsRoomSlab = [](const UPrimitiveComponent* Component)
	{
		if (!Component) return false;
		const FVector Extent = Component->Bounds.BoxExtent;
		// Ordinary floor slabs can have another storey's ceiling just below.
		// They are not raised furniture and should keep the normal lighting.
		return Extent.Z < 35.f && FMath::Min(Extent.X,Extent.Y) > 150.f && FMath::Max(Extent.X,Extent.Y) > 450.f;
	};
	if (IsRoomSlab(Support.GetComponent())) return false;
	// Look beneath the object being stood on. This also covers stairs without
	// hard-coding room elevations or relying on furniture asset names.
	Query.AddIgnoredActor(Support.GetActor());
	// A tool or small ornament may sit on a tabletop. Look through those
	// stacked supports instead of mistaking the tabletop for the room floor.
	for (int32 Layer = 0; Layer < 6; ++Layer)
	{
		if (!Holder->GetWorld()->LineTraceSingleByChannel(Below, Support.ImpactPoint + FVector::UpVector * 3.f,
			Support.ImpactPoint - FVector::UpVector * 280.f, ECC_Visibility, Query) || Below.ImpactNormal.Z < .65f) return false;
		if (Support.ImpactPoint.Z - Below.ImpactPoint.Z > 25.f) return true;
		if (IsRoomSlab(Below.GetComponent())) return false;
		Query.AddIgnoredActor(Below.GetActor());
	}
	return false;
}
}

UTallowCandleFlameComponent::UTallowCandleFlameComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTallowCandleFlameComponent::BeginPlay()
{
	Super::BeginPlay();
	Light = GetOwner()->FindComponentByClass<UPointLightComponent>();
	Candle = GetOwner()->FindComponentByClass<UTallowCandleComponent>();
	Keeper = Cast<ATallowCreature>(UGameplayStatics::GetActorOfClass(this, ATallowCreature::StaticClass()));
	// The Blueprint parks the candle mesh far below the camera, out of frame. Hold it in view instead,
	// with the flame light just above its wick.
	TArray<UStaticMeshComponent*> Meshes; GetOwner()->GetComponents(Meshes);
	for (UStaticMeshComponent* M : Meshes) if (M->GetName().Contains(TEXT("Candle"))) { HeldCandle = M; break; }
	UCameraComponent* View = nullptr;
	if (ATallowCharacter* Holder = Cast<ATallowCharacter>(GetOwner())) View = Holder->GetFirstPersonCameraComponent();
	if (HeldCandle)
	{
		// Parent to the camera so the candle stays in the lower right of frame wherever you look
		if (View) HeldCandle->AttachToComponent(View, FAttachmentTransformRules::KeepRelativeTransform);
		// Real candlestick prop (Poly Haven, CC0) in place of the Blueprint's placeholder
		if (UStaticMesh* Stick = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Props/Candle/wooden_candlestick_1k.wooden_candlestick_1k")))
		{
			HeldCandle->SetStaticMesh(Stick); HeldCandle->SetRelativeScale3D(FVector(1.f));
			WickHeight = Stick->GetBoundingBox().Max.Z + 3.f;
		}
		HeldCandle->SetRelativeLocationAndRotation(HeldOffset, FRotator::ZeroRotator);
		HeldCandle->SetCastShadow(false);
		HeldCandle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (Light.IsValid())
		{
			if (View) Light->AttachToComponent(View, FAttachmentTransformRules::KeepRelativeTransform);
			Light->SetRelativeLocation(HeldOffset + FVector(0.f, 0.f, WickHeight));
		}
	}
	if (ATallowCharacter* Player = Cast<ATallowCharacter>(GetOwner()))
	{
		if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
		{
			Controls = CreateWidget<UTallowControlsWidget>(PC, UTallowControlsWidget::StaticClass());
			Controls->Player = Player; Controls->AddToViewport(5);
		}
	}
	if (Light.IsValid())
	{
		BaseIntensity = Light->Intensity;
		BaseLocation = Light->GetRelativeLocation();
		Light->SetLightColor(FlameColour);
		Light->SetCastShadows(true);
	}
	Time = FMath::FRand() * 100.f;
	GustSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/FinalPass/Candle/S_Candle_Gust.S_Candle_Gust"));
	// The player's own scared breathing rises as the Keeper closes in (same proximity as the guttering)
	if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Player/S_Player_ScaredBreath.S_Player_ScaredBreath")))
	{
		Breath = NewObject<UAudioComponent>(GetOwner());
		Breath->SetSound(Sound); Breath->bAutoActivate = false; Breath->bAllowSpatialization = false;
		Breath->RegisterComponent(); Breath->SetVolumeMultiplier(0.001f);
	}
}

void UTallowCandleFlameComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bExternalPresentation && HeldCandle && Candle.IsValid()) HeldCandle->SetVisibility(Candle->IsAcquired());
	if (!Light.IsValid()) return;
	Time += DeltaTime;

	// How close is the Keeper? The flame reacts before the player can see it. Close means close by the way she
	// would have to come: through a wall she is further off than she looks (29 Oct: your candle betrays you).
	UpdateKeeperDistance(DeltaTime);
	const float Near = TallowCandleBetrayal::GutterForDistance(FeltDistance, GutterStartDistance, GutterFullDistance);
	KeeperGutter = FMath::FInterpTo(KeeperGutter, Near, DeltaTime, 2.5f);
	float Target = Near;
	if (Candle.IsValid() && Candle->IsGuttering()) Target = FMath::Max(Target, 0.7f);
	Gutter = FMath::FInterpTo(Gutter, Target, DeltaTime, 2.5f);
	UpdateGust(DeltaTime);
	const float GustNow = GetGustAmount();
	if (Breath)
	{
		// Holding her breath is dead silent: the scared breathing stops until she lets go.
		const auto* Owner = Cast<ATallowCharacter>(GetOwner());
		const bool bHolding = Owner && Owner->IsHoldingStill();
		const auto* Mode=GetWorld()->GetAuthGameMode<ATallowGameMode>();
		const bool bCaptured=Mode&&Mode->IsCaptureInProgress();
		// Breath only carries meaning when the Keeper is almost on top of her; guttering alone or a far Keeper stays silent.
		const float Dist = Keeper.IsValid() ? FVector::Dist(Keeper->GetActorLocation(), GetOwner()->GetActorLocation()) : 1e7f;
		const float Close = FMath::Clamp((CloseBreathStart - Dist) / FMath::Max(1.f, CloseBreathStart - CloseBreathFull), 0.f, 1.f);
		if (Close > 0.15f && !bHolding && !bCaptured && !Breath->IsPlaying()) Breath->Play();
		if ((Close < 0.05f || bHolding || bCaptured) && Breath->IsPlaying() && Breath->GetPlayState()!=EAudioComponentPlayState::FadingOut) Breath->FadeOut(.6f,0.f);
		Breath->SetVolumeMultiplier(FMath::Clamp(Close * .14f, 0.001f, .14f));
	}

	// Layered noise reads as flame rather than a faulty bulb: slow breathing, quick flutter, rare pops.
	const float Slow = FMath::PerlinNoise1D(Time * 1.3f);
	const float Fast = FMath::PerlinNoise1D(Time * 9.f + 17.f);
	// Holding her breath, the light steadies with her hand (28 Oct); only the flicker is calmed, never its strength.
	const auto* Breather = Cast<ATallowCharacter>(GetOwner());
	const float Amount = FlickerAmount * (1.f + 3.f * Gutter) * (Breather && Breather->IsHoldingStill() ? .3f + .9f * Breather->GetHeldStrain() : 1.f);
	float Scale = 1.f + Amount * (0.6f * Slow + 0.4f * Fast);

	// Guttering: the flame shrinks and sputters, sometimes nearly going out.
	Scale *= FMath::Lerp(1.f, 0.35f, Gutter);
	DropoutTimer -= DeltaTime;
	if (Gutter > 0.4f && DropoutTimer <= 0.f && FMath::FRand() < Gutter * DeltaTime * 3.f) DropoutTimer = FMath::FRandRange(0.05f, 0.2f);
	if (DropoutTimer > 0.f) Scale *= 0.15f;

	const bool bCupped = Candle.IsValid() && Candle->GetCandleState() == ECandleState::Cupped;
	if (bExternalPresentation)
	{
		// Preserve nearby floor and stair readability while the flame gutters. Cupping still narrows the gameplay radius.
		Scale = FMath::Max(.42f, Scale) * (bCupped ? .72f : 1.f);
	}
	// The gust: the light all but goes, sputtering, in the moment before the flame does.
	if (GustNow > 0.f) Scale *= FMath::Lerp(1.f, .1f + .3f * FMath::Abs(FMath::PerlinNoise1D(Time * 31.f)), GustNow);
	// Seen: the light dips and goes cold at the call, and stays restless and cool while it keeps looking.
	if (const auto* Seer = Cast<ATallowCharacter>(GetOwner()))
	{
		const float Call = FMath::Exp(-FMath::Max(0.f, GetWorld()->GetTimeSeconds() - Seer->SeenCallAt) / .6f);
		Seen = FMath::FInterpTo(Seen, FMath::Max(Call, FMath::Clamp(Seer->KeeperSight, 0.f, 1.f) * .7f), DeltaTime, Call > .2f ? 20.f : 2.f);
		Scale *= 1.f - .3f * Seen - .25f * Call * FMath::Abs(FMath::PerlinNoise1D(Time * 17.f));
		Light->SetLightColor(FMath::Lerp(FlameColour, FLinearColor(.55f, .68f, 1.f), Seen));
	}
	// A breath from nowhere lies the flame down past anything the integrated presentation keeps for the floor, then lets it stand.
	if (const float Down = GetLayDownAmount(); Down > 0.f)
	{
		Scale *= FMath::Lerp(1.f, .1f, Down);
		if (Down > .6f && DropoutTimer <= 0.f && FMath::FRand() < DeltaTime * 6.f) DropoutTimer = FMath::FRandRange(.05f, .12f);
		if (Down > .6f && DropoutTimer > 0.f) Scale *= .35f;
	}
	const float Wax=Candle.IsValid()?Candle->GetWaxFraction():1.f;
	const float WaxStrength=FMath::Lerp(.28f,1.f,FMath::Clamp(Wax/.35f,0.f,1.f));
	if(Wax<.15f) Scale*=.4f+.6f*FMath::Abs(FMath::PerlinNoise1D(Time*13.f));
	Light->SetIntensity((LumensOverride > 0.f ? LumensOverride : BaseIntensity) * FMath::Max(0.02f, Scale)*WaxStrength);
	// A struck match flares bright for a moment, whether the candle is out or not, and Mim can see her by it
	// (UTallowCandleComponent::GetMatchFlare). Out, the candle's light is shown for the flare and hidden after.
	const float Flare = Candle.IsValid() && Candle->IsAcquired() && !Candle->IsGivenUp() ? Candle->GetMatchFlare() : 0.f;
	const bool bCandleOut = !Candle.IsValid() || Candle->GetCandleState() == ECandleState::Out;
	if (Flare > 0.f)
	{
		if (bCandleOut) { Light->SetVisibility(true); bFlareShown = true; }
		Light->SetIntensity(FMath::Max(Light->Intensity, (LumensOverride > 0.f ? LumensOverride : BaseIntensity) * MatchFlareBoost * Flare));
	}
	else if (bFlareShown)
	{
		bFlareShown = false;
		if (bCandleOut) Light->SetVisibility(false);
	}
	// The flame leans and trembles; the light source moves a few millimetres with it, and is pushed a little away from
	// the way the Keeper is coming.
	const FVector Lean(FMath::PerlinNoise1D(Time * 2.1f + 3.f), FMath::PerlinNoise1D(Time * 2.3f + 7.f), 0.f);
	const FVector Push = Light->GetAttachParent() ? Light->GetAttachParent()->GetComponentTransform().InverseTransformVectorNoScale(AwayFromKeeper) : AwayFromKeeper;
	const FVector FlameOffset = BaseLocation + Lean * (0.6f + 2.f * Gutter) + Push * (2.5f * KeeperGutter + 4.f * GustNow) * (bCupped ? .3f : 1.f)
		+ FVector(2.5f, 0.f, -1.5f) * GetLayDownAmount();
	const auto* Holder = Cast<ATallowCharacter>(GetOwner());
	const auto* Camera = Holder ? Holder->GetFirstPersonCameraComponent() : nullptr;
	if (bExternalPresentation && Camera && Light->GetAttachParent())
	{
		const FVector WickPosition = Light->GetAttachParent()->GetComponentTransform().TransformPosition(FlameOffset);
		Light->SetWorldLocation(VisibleCandleLightPosition(GetOwner(), Camera, WickPosition, ViewHoldDistance));
	}
	else Light->SetRelativeLocation(FlameOffset);
	if (PresentationLight)
	{
		// The projected first-person mesh sits centimetres from the emitter.
		// Its separate lighting channel prevents clipped hands without weakening
		// the actual candle that illuminates the house and informs Mim's routing.
		const bool bLit = Candle.IsValid() && Candle->IsAcquired() && (Candle->GetCandleState() != ECandleState::Out || Flare > 0.f);
		PresentationLight->SetVisibility(bLit);
		PresentationLight->SetWorldLocation(Light->GetComponentLocation());
		PresentationLight->SetIntensity(Light->Intensity * .04f);
		PresentationLight->SetLightColor(Light->GetLightColor());
	}
	if (NavigationSpill)
	{
		const bool bLit = Candle.IsValid() && Candle->IsAcquired() && Candle->GetCandleState() != ECandleState::Out;
		const bool bRaised = bLit && Holder && IsStandingAboveRoomFloor(Holder);
		SpillAlpha = FMath::FInterpTo(SpillAlpha, bRaised ? 1.f : 0.f, DeltaTime, 6.f);
		// A restrained diffuse bounce keeps the floor below a table/stair readable.
		// The main flame keeps its shadows, fuel response and existing radius.
		NavigationSpill->SetVisibility(bLit && SpillAlpha > .01f);
		NavigationSpill->SetIntensity(BaseIntensity * .25f * FMath::Max(.5f, Scale * WaxStrength) * SpillAlpha);
		NavigationSpill->SetAttenuationRadius(Light->AttenuationRadius);
		NavigationSpill->SetLightColor(Light->GetLightColor());
	}
}

void UTallowCandleFlameComponent::UpdateKeeperDistance(float DeltaTime)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (!Keeper.IsValid() && Now >= NextKeeperSearch)
	{
		NextKeeperSearch = Now + 1.f;
		Keeper = Cast<ATallowCreature>(UGameplayStatics::GetActorOfClass(this, ATallowCreature::StaticClass()));
	}
	// Not in the house yet (asleep, hidden for a beat): the flame is calm.
	if (!Keeper.IsValid() || Keeper->IsHidden()) { FeltDistance = 100000.f; KeeperClosing = 0.f; return; }
	const FVector Her = Keeper->GetActorLocation(), Nell = GetOwner()->GetActorLocation();
	const float Straight = FVector::Dist(Her, Nell);
	if (Straight > GutterStartDistance * 1.5f)
	{
		// Too far for any walk to bring her inside the gutter: no path query needed.
		FeltDistance = Straight;
		TowardNell = (Nell - Her).GetSafeNormal();
		AwayTarget = (Nell - Her).GetSafeNormal2D();
	}
	else if (Now >= NextKeeperQuery)
	{
		NextKeeperQuery = Now + .25f;
		FeltDistance = MeasureKeeperDistance(Her, Nell, Keeper->GetController(), &AwayTarget, &TowardNell);
	}
	if (!AwayTarget.IsNearlyZero()) AwayFromKeeper = FMath::VInterpTo(AwayFromKeeper, AwayTarget, DeltaTime, 4.f).GetSafeNormal2D();
	// Coming or going, along her first step toward Nell.
	KeeperClosing = FVector::DotProduct(Keeper->GetVelocity(), TowardNell);
}

float UTallowCandleFlameComponent::MeasureKeeperDistance(const FVector& From, const FVector& To, AController* Walker, FVector* OutAway, FVector* OutToward) const
{
	const float Straight = FVector::Dist(From, To);
	FVector Away = (To - From).GetSafeNormal2D(), Toward = (To - From).GetSafeNormal();
	float Walk = 0.f;
	UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), From, To, Walker);
	if (Path && Path->IsValid() && !Path->IsPartial() && Path->PathPoints.Num() > 1)
	{
		const TArray<FVector>& Points = Path->PathPoints;
		Walk = Path->GetPathLength();
		Toward = (Points[1] - Points[0]).GetSafeNormal();
		// The air she pushes ahead of her comes in the way she does: through the last doorway, round the last corner.
		for (int32 I = Points.Num() - 1; I > 0; --I)
			if (FVector::Dist2D(Points[I], Points[I - 1]) > 20.f) { Away = (Points[I] - Points[I - 1]).GetSafeNormal2D(); break; }
	}
	// No walk (on a ceiling, behind a barred door, off the navmesh): the walls between still keep her further off.
	float Through = 1.f;
	if (Walk <= 0.f)
		if (const UTallowNoiseSubsystem* Noise = UTallowNoiseSubsystem::Get(this)) Through = Noise->GetOcclusion(From, To, GetOwner());
	if (OutAway && !Away.IsNearlyZero()) *OutAway = Away;
	if (OutToward && !Toward.IsNearlyZero()) *OutToward = Toward;
	return TallowCandleBetrayal::FeltDistance(Straight, Walk, Through);
}

bool UTallowCandleFlameComponent::IsGustHeld() const
{
	const UWorld* World = GetWorld();
	const auto* Holder = Cast<ATallowCharacter>(GetOwner());
	if (!World || !Holder || Holder->IsTiedToChair() || Holder->IsHiddenInSpot()) return true;
	if (const auto* Mode = World->GetAuthGameMode<ATallowGameMode>(); Mode && Mode->IsCaptureInProgress()) return true;
	// Protected moments (reading, threading, the screening) and the sequences Mim is not awake for.
	if (const auto* Pacing = World->GetSubsystem<UTallowHunterDirector>(); Pacing && (Pacing->IsProtected() || Pacing->GetAct() == ETallowStoryAct::Dormant)) return true;
	if (const auto* Chapters = World->GetSubsystem<UTallowChapterSubsystem>(); Chapters && Chapters->GetActiveBeat() != ETallowChapterBeat::None) return true;
	if (const auto* Scares = UTallowScareDirector::Get(this); Scares && Scares->IsCuePlaying()) return true;
	// Her authored moments: the still heap, the reveal out of it, the grab, the stand-off watch.
	if (const auto* Hunter = Keeper.IsValid() ? Cast<ATallowHunterAIController>(Keeper->GetController()) : nullptr)
	{
		const ETallowKeeperIntent Intent = Hunter->GetIntent();
		if (Intent == ETallowKeeperIntent::Disguise || Intent == ETallowKeeperIntent::Reveal || Intent == ETallowKeeperIntent::Lunge || Hunter->IsWatching()) return true;
	}
	return false;
}

float UTallowCandleFlameComponent::GetGustAmount() const
{
	if (GustRemaining <= 0.f || GustSeconds <= 0.f) return 0.f;
	// It comes on fast and holds the flame flat until it goes.
	return FMath::Clamp((GustSeconds - GustRemaining) / (.3f * GustSeconds), 0.f, 1.f);
}

void UTallowCandleFlameComponent::UpdateGust(float DeltaTime)
{
	if (!Candle.IsValid()) return;
	Gust.StartCm = GustStartDistance; Gust.SureCm = GustSureDistance; Gust.MaxRatePerSecond = GustRatePerSecond;
	Gust.EncounterCm = GutterStartDistance;
	if (GustRemaining > 0.f)
	{
		GustRemaining -= DeltaTime;
		// Cupping in time saves it: GustOut leaves a cupped flame alone.
		if (GustRemaining <= 0.f) { GustRemaining = 0.f; if (!IsGustHeld()) Candle->GustOut(); }
		return;
	}
	const bool bExposed = Candle->IsAcquired() && !Candle->IsGivenUp() && Candle->GetCandleState() == ECandleState::Lit && !Candle->IsRelighting();
	if (Gust.Step(FeltDistance, KeeperClosing, DeltaTime, bExposed, bExposed && IsGustHeld(), FMath::FRand()))
	{
		GustRemaining = GustSeconds;
		if (GustSound) UGameplayStatics::PlaySound2D(this, GustSound, GustVolume, FMath::FRandRange(.94f, 1.06f));
	}
}

void UTallowCandleFlameComponent::UseExternalPresentation(USceneComponent* Wick, float LightIntensity)
{
	if (!Wick || Wick->GetOwner() != GetOwner()) return;
	bExternalPresentation = true;
	if (HeldCandle) HeldCandle->SetVisibility(false);
	if (Controls) Controls->RemoveFromParent();
	if (Light.IsValid())
	{
		Light->AttachToComponent(Wick, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		BaseLocation = FVector::ZeroVector;
		Light->SetIntensityUnits(ELightUnits::Lumens);
		Light->bUseInverseSquaredFalloff = true;
		BaseIntensity = FMath::Max(.1f, LightIntensity);
		Light->SetIntensity(BaseIntensity);
	}
	if (!PresentationLight)
	{
		PresentationLight = NewObject<UPointLightComponent>(GetOwner(), TEXT("FirstPersonIllumination"));
		PresentationLight->SetupAttachment(Wick);
		PresentationLight->SetMobility(EComponentMobility::Movable);
		PresentationLight->SetLightingChannels(false, true, false);
		PresentationLight->SetIntensityUnits(ELightUnits::Lumens);
		PresentationLight->SetIntensity(0.f);
		PresentationLight->SetAttenuationRadius(100.f);
		PresentationLight->SetSourceRadius(6.f);
		PresentationLight->SetSoftSourceRadius(10.f);
		PresentationLight->SetCastShadows(false);
		PresentationLight->SetSpecularScale(.25f);
		PresentationLight->SetIndirectLightingIntensity(0.f);
		PresentationLight->SetVolumetricScatteringIntensity(0.f);
		GetOwner()->AddInstanceComponent(PresentationLight);
		PresentationLight->RegisterComponent();
	}
	if (!NavigationSpill)
		if (const auto* Holder = Cast<ATallowCharacter>(GetOwner()))
		{
			NavigationSpill = NewObject<UPointLightComponent>(GetOwner(), TEXT("NavigationSpill"));
			NavigationSpill->SetupAttachment(Holder->GetFirstPersonCameraComponent());
			NavigationSpill->SetRelativeLocation(FVector(8.f,0.f,-18.f));
			NavigationSpill->SetMobility(EComponentMobility::Movable);
			NavigationSpill->SetIntensityUnits(ELightUnits::Lumens);
			NavigationSpill->SetIntensity(0.f); NavigationSpill->SetVisibility(false);
			NavigationSpill->SetCastShadows(false); NavigationSpill->SetSpecularScale(0.f);
			NavigationSpill->SetIndirectLightingIntensity(0.f); NavigationSpill->SetVolumetricScatteringIntensity(0.f);
			GetOwner()->AddInstanceComponent(NavigationSpill); NavigationSpill->RegisterComponent();
		}
}

void UTallowCandleFlameComponent::LayDown(float Seconds)
{
	LayDownStarted = Time; LayDownSeconds = FMath::Max(Seconds, .2f);
}

float UTallowCandleFlameComponent::GetLayDownAmount() const
{
	// Down in a breath, held there, then up again more slowly than it went.
	const float T = LayDownSeconds > 0.f ? (Time - LayDownStarted) / LayDownSeconds : 2.f;
	if (T < 0.f || T >= 1.f) return 0.f;
	return T < .1f ? FMath::SmoothStep(0.f, .1f, T) : T < .55f ? 1.f : 1.f - FMath::SmoothStep(.55f, 1.f, T);
}
