#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TallowSteppedAnimationComponent.generated.h"

class USkeletalMeshComponent;

/**
 *  Animates the owner's skeletal meshes on twos, the way stop motion is shot. The pose only updates
 *  a set number of times a second while the actor still moves smoothly through the world.
 */
UCLASS(ClassGroup = (Tallow), meta = (BlueprintSpawnableComponent))
class TALLOW_API UTallowSteppedAnimationComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	virtual void BeginPlay() override;

	/** Turns stepping on or off, for example off during the staged transformation */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void SetStepped(bool bNewStepped);

	/** Changes how many poses a second are shown */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	void SetStepRate(float NewStepRate);

	UFUNCTION(BlueprintPure, Category = "Animation")
	bool IsStepped() const { return bStepped; }

protected:

	/** Poses shown per second, 12 is on twos at 24fps */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation", meta = (ClampMin = "1.0", ClampMax = "60.0"))
	float StepRate = 12.0f;

	/** Whether the animation starts stepped */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	bool bStepped = true;

	/** Applies the step rate to every skeletal mesh on the owner */
	void ApplyStepping();

	/** Skeletal meshes being stepped */
	UPROPERTY()
	TArray<TObjectPtr<USkeletalMeshComponent>> SteppedMeshes;
};
