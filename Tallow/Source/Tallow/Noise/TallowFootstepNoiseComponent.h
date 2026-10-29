#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TallowFootstepNoiseComponent.generated.h"

class ACharacter;

/**
 *  Makes footstep noises as a character moves. Walking, crouching and running each use their own noise type,
 *  and landing from a fall makes a noise scaled by how hard you hit the ground.
 */
UCLASS(ClassGroup = (Tallow), meta = (BlueprintSpawnableComponent))
class TALLOW_API UTallowFootstepNoiseComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	/** Constructor */
	UTallowFootstepNoiseComponent();

	virtual void BeginPlay() override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Makes a single step noise now, for use from animation notifies */
	UFUNCTION(BlueprintCallable, Category = "Noise")
	void EmitStep();

	/** Distance in cm between steps, so the walk camera can bob on the same cadence */
	float GetStrideLength() const { return StrideLength; }

protected:

	/** Distance travelled in cm between steps */
	UPROPERTY(EditAnywhere, Category = "Noise", meta = (ClampMin = "10.0", Units = "cm"))
	float StrideLength = 60.0f;

	/** Speeds above this count as running */
	UPROPERTY(EditAnywhere, Category = "Noise", meta = (Units = "cm/s"))
	float RunSpeedThreshold = 350.0f;

	/** Falls slower than this make no landing noise */
	UPROPERTY(EditAnywhere, Category = "Noise", meta = (Units = "cm/s"))
	float MinLandingSpeed = 300.0f;

	/** Falls this fast or faster make a full volume landing noise */
	UPROPERTY(EditAnywhere, Category = "Noise", meta = (Units = "cm/s"))
	float MaxLandingSpeed = 900.0f;

	/** Leave on for the player, turn off for the creature so it doesn't hear itself */
	UPROPERTY(EditAnywhere, Category = "Noise")
	bool bAlertsAI = true;

	/** Leave off when footsteps come from animation notifies instead */
	UPROPERTY(EditAnywhere, Category = "Noise")
	bool bAutomaticSteps = true;

	/** Called when the owning character lands */
	UFUNCTION()
	void HandleLanded(const FHitResult& Hit);

	/** Owning character */
	UPROPERTY()
	TObjectPtr<ACharacter> OwnerCharacter;

	/** Distance moved since the last step */
	float DistanceSinceStep = 0.0f;

	/** Vertical speed last tick, used to measure landing impact */
	float LastVerticalSpeed = 0.0f;
};
