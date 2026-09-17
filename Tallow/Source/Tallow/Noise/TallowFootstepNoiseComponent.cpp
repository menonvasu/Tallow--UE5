#include "Noise/TallowFootstepNoiseComponent.h"
#include "Noise/TallowNoiseSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UTallowFootstepNoiseComponent::UTallowFootstepNoiseComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTallowFootstepNoiseComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (OwnerCharacter)
	{
		OwnerCharacter->LandedDelegate.AddDynamic(this, &UTallowFootstepNoiseComponent::HandleLanded);
	}
	else
	{
		SetComponentTickEnabled(false);
	}
}

void UTallowFootstepNoiseComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!OwnerCharacter)
	{
		return;
	}

	// Remember fall speed for the landing noise, the velocity is already zeroed by the time Landed fires
	LastVerticalSpeed = OwnerCharacter->GetVelocity().Z;

	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	if (!bAutomaticSteps || !Movement || !Movement->IsMovingOnGround())
	{
		return;
	}

	DistanceSinceStep += OwnerCharacter->GetVelocity().Size2D() * DeltaTime;
	if (DistanceSinceStep >= StrideLength)
	{
		DistanceSinceStep = 0.0f;
		EmitStep();
	}
}

void UTallowFootstepNoiseComponent::EmitStep()
{
	UTallowNoiseSubsystem* NoiseSubsystem = UTallowNoiseSubsystem::Get(this);
	if (!NoiseSubsystem || !OwnerCharacter)
	{
		return;
	}

	ETallowNoiseType Type = ETallowNoiseType::Footstep;
	if (OwnerCharacter->bIsCrouched)
	{
		Type = ETallowNoiseType::CrouchStep;
	}
	else if (OwnerCharacter->GetVelocity().Size2D() > RunSpeedThreshold)
	{
		Type = ETallowNoiseType::RunStep;
	}

	// Noise comes from just above the feet rather than the middle of the capsule
	const FVector FootLocation = OwnerCharacter->GetActorLocation() - FVector(0.0f, 0.0f, OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 10.0f);

	NoiseSubsystem->EmitNoise(Type, FootLocation, OwnerCharacter, 1.0f, bAlertsAI);
}

void UTallowFootstepNoiseComponent::HandleLanded(const FHitResult& Hit)
{
	const float ImpactSpeed = FMath::Abs(LastVerticalSpeed);
	if (ImpactSpeed < MinLandingSpeed)
	{
		return;
	}

	if (UTallowNoiseSubsystem* NoiseSubsystem = UTallowNoiseSubsystem::Get(this))
	{
		const float Scale = static_cast<float>(FMath::GetMappedRangeValueClamped(FVector2D(MinLandingSpeed, MaxLandingSpeed), FVector2D(0.4f, 1.0f), ImpactSpeed));
		NoiseSubsystem->EmitNoise(ETallowNoiseType::Land, Hit.ImpactPoint + FVector(0.0f, 0.0f, 10.0f), OwnerCharacter, Scale, bAlertsAI);
	}

	DistanceSinceStep = 0.0f;
}
