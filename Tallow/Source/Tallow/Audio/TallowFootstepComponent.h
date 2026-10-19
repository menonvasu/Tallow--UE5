#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TallowFootstepComponent.generated.h"

class USoundBase;
class USoundAttenuation;

/** Steps by distance actually walked on the ground: one cue per stride, louder and longer-strided when running,
 *  hushed when crouched, with an occasional extra layered on (a floorboard creak for the child, the Keeper's
 *  dragging hem). FalloffCm 0 plays it unspatialised for the player's own feet. Interim CC0 cues; see CREDITS.md. */
UCLASS(ClassGroup=(Tallow), meta=(BlueprintSpawnableComponent))
class TALLOW_API UTallowFootstepComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTallowFootstepComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, Category = "Footsteps") TArray<TObjectPtr<USoundBase>> Steps;
	UPROPERTY(EditAnywhere, Category = "Footsteps") TArray<TObjectPtr<USoundBase>> Extras;
	UPROPERTY(EditAnywhere, Category = "Footsteps") float StrideCm = 55.f;
	UPROPERTY(EditAnywhere, Category = "Footsteps") float Volume = .4f;
	UPROPERTY(EditAnywhere, Category = "Footsteps") float ExtraChance = .1f;
	UPROPERTY(EditAnywhere, Category = "Footsteps") float ExtraVolume = .3f;
	/** Distance (cm) at which the steps fade out; 0 = the listener's own feet, not spatialised */
	UPROPERTY(EditAnywhere, Category = "Footsteps") float FalloffCm = 0.f;

private:
	void Play(USoundBase* Sound, const FVector& Where, float Gain) const;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> Attenuation;
	float Travelled = 0.f;
	int32 LastStep = -1;
};
