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
class USceneComponent;

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
	float GetGutterAmount() const { return FMath::Max(Gutter, FMath::Max(GetGustAmount(), GetLayDownAmount())); }
	/** A breath of air from nowhere (a minor scare): for Seconds the flame lies down and all but goes out, then stands up
	 *  again. Only the look and the light of it; the candle's state, and what Mim can see of it, are untouched. */
	UFUNCTION(BlueprintCallable, Category = "Candle|Flame")
	void LayDown(float Seconds);
	float GetLayDownAmount() const;
	/** The integrated presentation owns the prop and wick; retain this component's existing gutter/light rules. */
	UFUNCTION(BlueprintCallable, Category="Candle|Flame")
	void UseExternalPresentation(USceneComponent* Wick, float LightIntensity = 18.f);

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
	FLinearColor FlameColour = FLinearColor(1.f, 0.70f, 0.42f);

	/** While positive, the flame's light stays within this many cm in front of the eye (the capture shot
	 *  brings Mim's face closer than the held candle; the light must not end up inside her head). */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "Candle|Flame")
	float ViewHoldDistance = 0.f;

	/** While positive, the flame's light is this many lumens instead of its own (the capture shot lights her face
	 *  at the lens with the same few lumens whether the candle is lit or out). */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "Candle|Flame")
	float LumensOverride = 0.f;

private:
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> NavigationSpill;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> PresentationLight;
	float SpillAlpha = 0.f;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Breath;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> HeldCandle;
	UPROPERTY(Transient) TObjectPtr<UTallowControlsWidget> Controls;
	TWeakObjectPtr<UPointLightComponent> Light;
	TWeakObjectPtr<UTallowCandleComponent> Candle;
	TWeakObjectPtr<ATallowCreature> Keeper;
	float BaseIntensity = 0.f;
	FVector BaseLocation = FVector::ZeroVector;
	float Gutter = 0.f;
	float Seen = 0.f;
	float Time = 0.f;
	float DropoutTimer = 0.f;
	bool bExternalPresentation = false;
};
