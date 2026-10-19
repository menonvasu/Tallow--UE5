#include "Audio/TallowFootstepComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

UTallowFootstepComponent::UTallowFootstepComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTallowFootstepComponent::BeginPlay()
{
	Super::BeginPlay();
	if (FalloffCm <= 0.f) return;
	Attenuation = NewObject<USoundAttenuation>(this);
	Attenuation->Attenuation.bAttenuate = true;
	Attenuation->Attenuation.bSpatialize = true;
	Attenuation->Attenuation.AttenuationShapeExtents = FVector(100.f);
	Attenuation->Attenuation.FalloffDistance = FalloffCm;
}

void UTallowFootstepComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || Steps.IsEmpty() || !Character->GetCharacterMovement() || !Character->GetCharacterMovement()->IsMovingOnGround()) return;
	const float Speed = Character->GetVelocity().Size2D();
	// Standing still primes the first step to land soon after you set off.
	if (Speed < 15.f) { Travelled = StrideCm * .6f; return; }
	Travelled += Speed * DeltaTime;
	const float Stride = StrideCm * FMath::GetMappedRangeValueClamped(FVector2D(100.f, 450.f), FVector2D(1.f, 1.6f), Speed);
	if (Travelled < Stride) return;
	Travelled -= Stride;

	int32 Index = FMath::RandRange(0, Steps.Num() - 1);
	if (Steps.Num() > 1 && Index == LastStep) Index = (Index + 1) % Steps.Num();
	LastStep = Index;
	const float Gain = FMath::GetMappedRangeValueClamped(FVector2D(60.f, 400.f), FVector2D(.5f, 1.3f), Speed) * (Character->bIsCrouched ? .45f : 1.f);
	const FVector Feet = Character->GetActorLocation() - FVector(0.f, 0.f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	Play(Steps[Index], Feet, Volume * Gain);
	if (!Extras.IsEmpty() && FMath::FRand() < ExtraChance)
		Play(Extras[FMath::RandRange(0, Extras.Num() - 1)], Feet, ExtraVolume * Gain);
}

void UTallowFootstepComponent::Play(USoundBase* Sound, const FVector& Where, float Gain) const
{
	if (!Sound) return;
	const float Pitch = FMath::FRandRange(.94f, 1.06f);
	if (Attenuation) UGameplayStatics::PlaySoundAtLocation(this, Sound, Where, Gain, Pitch, 0.f, Attenuation);
	else UGameplayStatics::PlaySound2D(this, Sound, Gain, Pitch);
}
