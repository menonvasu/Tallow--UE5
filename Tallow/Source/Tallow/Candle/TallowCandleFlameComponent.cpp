#include "Candle/TallowCandleFlameComponent.h"
#include "Candle/TallowCandleComponent.h"
#include "AI/TallowCreature.h"
#include "Components/PointLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMesh.h"
#include "UI/TallowControlsWidget.h"
#include "Player/TallowCharacter.h"
#include "GameFramework/PlayerController.h"

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

	// How close is the Keeper? The flame reacts before the player can see it.
	float Target = 0.f;
	if (Keeper.IsValid())
	{
		const float Dist = FVector::Dist(Keeper->GetActorLocation(), GetOwner()->GetActorLocation());
		Target = FMath::Clamp((GutterStartDistance - Dist) / FMath::Max(1.f, GutterStartDistance - GutterFullDistance), 0.f, 1.f);
	}
	if (Candle.IsValid() && Candle->IsGuttering()) Target = FMath::Max(Target, 0.7f);
	Gutter = FMath::FInterpTo(Gutter, Target, DeltaTime, 2.5f);
	if (Breath)
	{
		if (Gutter > 0.08f && !Breath->IsPlaying()) Breath->Play();
		if (Gutter < 0.03f && Breath->IsPlaying()) Breath->Stop();
		Breath->SetVolumeMultiplier(FMath::Clamp(Gutter * 1.2f, 0.001f, 1.f));
	}

	// Layered noise reads as flame rather than a faulty bulb: slow breathing, quick flutter, rare pops.
	const float Slow = FMath::PerlinNoise1D(Time * 1.3f);
	const float Fast = FMath::PerlinNoise1D(Time * 9.f + 17.f);
	const float Amount = FlickerAmount * (1.f + 3.f * Gutter);
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
	Light->SetIntensity(BaseIntensity * FMath::Max(0.02f, Scale));
	// The flame leans and trembles; the light source moves a few millimetres with it.
	const FVector Lean(FMath::PerlinNoise1D(Time * 2.1f + 3.f), FMath::PerlinNoise1D(Time * 2.3f + 7.f), 0.f);
	Light->SetRelativeLocation(BaseLocation + Lean * (0.6f + 2.f * Gutter));
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
}
