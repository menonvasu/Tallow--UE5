#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TallowCandleFlameComponent.generated.h"

class UPointLightComponent;
class UTallowCandleComponent;
class ATallowCreature;
class UAudioComponent;
class UStaticMeshComponent;
class UTallowControlsWidget;

/** Makes the candle's point light read as a living flame: layered flicker, a slight lean,
 *  and guttering (dimming, sputtering) as the Keeper gets close. Works alongside the candle state machine. */
UCLASS(ClassGroup=(Tallow), meta=(BlueprintSpawnableComponent))
class TALLOW_API UTallowCandleFlameComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTallowCandleFlameComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 0 = calm flame, 1 = fully guttering */
	UFUNCTION(BlueprintPure, Category = "Candle|Flame")
	float GetGutterAmount() const { return Gutter; }

	/** Beyond this distance (cm) the Keeper has no effect on the flame */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Candle|Flame")
	float GutterStartDistance = 700.f;

	/** At or inside this distance (cm) the flame gutters fully */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Candle|Flame")
	float GutterFullDistance = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Candle|Flame")
	float FlickerAmount = 0.14f;

	/** Where the held candle sits relative to the camera: forward, right, down (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Candle|Flame")
	FVector HeldOffset = FVector(45.f, 26.f, -30.f);
	float WickHeight = 13.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Candle|Flame")
	FLinearColor FlameColour = FLinearColor(1.f, 0.55f, 0.22f);

private:
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Breath;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> HeldCandle;
	UPROPERTY(Transient) TObjectPtr<UTallowControlsWidget> Controls;
	TWeakObjectPtr<UPointLightComponent> Light;
	TWeakObjectPtr<UTallowCandleComponent> Candle;
	TWeakObjectPtr<ATallowCreature> Keeper;
	float BaseIntensity = 0.f;
	FVector BaseLocation = FVector::ZeroVector;
	float Gutter = 0.f;
	float Time = 0.f;
	float DropoutTimer = 0.f;
};
