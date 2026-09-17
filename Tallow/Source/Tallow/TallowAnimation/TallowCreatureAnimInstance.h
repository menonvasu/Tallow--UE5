#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Noise/TallowNoiseTypes.h"
#include "TallowCreatureAnimInstance.generated.h"

class ACharacter;
class UAnimMontage;

/**
 *  C++ base for the creature Animation Blueprint. Works out movement values every frame and reacts
 *  to noises the creature can hear by turning its head and playing a reaction montage.
 */
UCLASS()
class TALLOW_API UTallowCreatureAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	virtual void NativeInitializeAnimation() override;

	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	virtual void NativeUninitializeAnimation() override;

protected:

	/** Called on the frame the creature hears a noise, for any extra reaction set up in the Animation Blueprint */
	UFUNCTION(BlueprintImplementableEvent, Category = "Animation|Noise")
	void OnNoiseHeard(FVector NoiseLocation, float Loudness, ETallowNoiseType NoiseType);

	/** Ground speed in cm/s */
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement")
	float Speed = 0.0f;

	/** Movement direction relative to facing, from -180 to 180 */
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement")
	float Direction = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement")
	bool bIsMoving = false;

	UPROPERTY(BlueprintReadOnly, Category = "Animation|Movement")
	bool bIsFalling = false;

	/** World location the head should turn towards, feed into a Look At node */
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Noise")
	FVector LookAtLocation = FVector::ZeroVector;

	/** How strongly the head turns towards LookAtLocation, 0 to 1 */
	UPROPERTY(BlueprintReadOnly, Category = "Animation|Noise")
	float LookAtAlpha = 0.0f;

	/** Speed above which the creature counts as moving */
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Movement")
	float MovingSpeedThreshold = 5.0f;

	/** Noises quieter than this at the creature's head are ignored */
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Noise", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HearingThreshold = 0.02f;

	/** How long the head stays turned towards a noise, in seconds */
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Noise")
	float LookAtHoldTime = 2.5f;

	/** How fast the head turns towards and away from a noise */
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Noise")
	float LookAtBlendSpeed = 4.0f;

	/** Montage played when a noise is heard, such as a head snap. Optional */
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Noise")
	TObjectPtr<UAnimMontage> NoiseReactionMontage;

	/** Noises at least this loud play the reaction montage */
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Noise", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReactionMontageThreshold = 0.15f;

	/** Minimum seconds between reaction montages */
	UPROPERTY(EditDefaultsOnly, Category = "Animation|Noise")
	float ReactionCooldown = 3.0f;

	/** Called by the noise subsystem for every noise made */
	UFUNCTION()
	void HandleNoiseEmitted(const FTallowNoiseEvent& NoiseEvent);

	/** Owning creature */
	UPROPERTY()
	TObjectPtr<ACharacter> OwnerCharacter;

	/** World time the last noise was heard */
	double LastHeardTime = -1000.0;

	/** World time the last reaction montage played */
	double LastReactionTime = -1000.0;
};
