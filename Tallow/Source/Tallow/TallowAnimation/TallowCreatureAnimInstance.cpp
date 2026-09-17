#include "TallowAnimation/TallowCreatureAnimInstance.h"
#include "Noise/TallowNoiseSubsystem.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void UTallowCreatureAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	OwnerCharacter = Cast<ACharacter>(TryGetPawnOwner());

	// Only listen in a game world, the noise subsystem doesn't exist in the animation editor preview
	if (OwnerCharacter)
	{
		if (UTallowNoiseSubsystem* NoiseSubsystem = UTallowNoiseSubsystem::Get(OwnerCharacter.Get()))
		{
			NoiseSubsystem->OnNoiseEmitted.AddUniqueDynamic(this, &UTallowCreatureAnimInstance::HandleNoiseEmitted);
		}
	}
}

void UTallowCreatureAnimInstance::NativeUninitializeAnimation()
{
	if (UTallowNoiseSubsystem* NoiseSubsystem = UTallowNoiseSubsystem::Get(OwnerCharacter.Get()))
	{
		NoiseSubsystem->OnNoiseEmitted.RemoveDynamic(this, &UTallowCreatureAnimInstance::HandleNoiseEmitted);
	}

	Super::NativeUninitializeAnimation();
}

void UTallowCreatureAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!OwnerCharacter)
	{
		return;
	}

	// Movement
	const FVector Velocity = OwnerCharacter->GetVelocity();
	Speed = Velocity.Size2D();
	bIsMoving = Speed > MovingSpeedThreshold;
	bIsFalling = OwnerCharacter->GetCharacterMovement() && OwnerCharacter->GetCharacterMovement()->IsFalling();

	if (bIsMoving)
	{
		const FRotator Facing = OwnerCharacter->GetActorRotation();
		Direction = static_cast<float>(FRotator::NormalizeAxis(Velocity.ToOrientationRotator().Yaw - Facing.Yaw));
	}

	// Hold the head on the last noise for a while, then let it go
	const double Now = OwnerCharacter->GetWorld()->GetTimeSeconds();
	const float TargetAlpha = (Now - LastHeardTime) < LookAtHoldTime ? 1.0f : 0.0f;
	LookAtAlpha = FMath::FInterpTo(LookAtAlpha, TargetAlpha, DeltaSeconds, LookAtBlendSpeed);
}

void UTallowCreatureAnimInstance::HandleNoiseEmitted(const FTallowNoiseEvent& NoiseEvent)
{
	if (!OwnerCharacter || !NoiseEvent.bAlertsAI || NoiseEvent.Instigator.Get() == OwnerCharacter.Get())
	{
		return;
	}

	UTallowNoiseSubsystem* NoiseSubsystem = UTallowNoiseSubsystem::Get(OwnerCharacter.Get());
	if (!NoiseSubsystem)
	{
		return;
	}

	// Same falloff and occlusion the AI uses, so the head only turns for noises the creature can actually hear
	const float Loudness = NoiseSubsystem->GetLoudnessAt(NoiseEvent, OwnerCharacter->GetPawnViewLocation());
	if (Loudness < HearingThreshold)
	{
		return;
	}

	const double Now = OwnerCharacter->GetWorld()->GetTimeSeconds();
	LookAtLocation = NoiseEvent.Location;
	LastHeardTime = Now;

	if (NoiseReactionMontage && Loudness >= ReactionMontageThreshold && (Now - LastReactionTime) >= ReactionCooldown && !Montage_IsPlaying(NoiseReactionMontage))
	{
		Montage_Play(NoiseReactionMontage);
		LastReactionTime = Now;
	}

	OnNoiseHeard(NoiseEvent.Location, Loudness, NoiseEvent.Type);
}
