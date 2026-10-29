#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Candle/TallowCandleBetrayal.h"
#include "TallowCandleFlameComponent.generated.h"

class UPointLightComponent;
class UTallowCandleComponent;
class ATallowCreature;
class UAudioComponent;
class USoundBase;
class UStaticMeshComponent;
class UTallowControlsWidget;
class USceneComponent;
class AController;

/** Makes the candle's point light read as a living flame: layered flicker, a slight lean,
 *  and guttering (dimming, sputtering) as the Keeper gets close. Works alongside the candle state machine.
 *  Your candle betrays you (29 Oct): how close she is goes by her walk, not through walls; the flame leans away
 *  from the way she is coming; right on top of Nell a gust can take an open flame; a struck match flares. */
UCLASS(ClassGroup=(Tallow), meta=(BlueprintSpawnableComponent))
class TALLOW_API UTallowCandleFlameComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTallowCandleFlameComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 0 = calm flame, 1 = fully guttering (a gust or a scare laying it down counts: the visible flame follows this) */
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

	/** The scared breathing is heard only this close to the Keeper (full by CloseBreathFull). */
	UPROPERTY(EditAnywhere, Category="Candle|Breath") float CloseBreathStart = 320.f;
	UPROPERTY(EditAnywhere, Category="Candle|Breath") float CloseBreathFull = 160.f;

	/** How far off the Keeper feels (cm): her walk to Nell, or the straight line stretched by the walls between. */
	UFUNCTION(BlueprintPure, Category = "Candle|Flame")
	float GetKeeperDistance() const { return FeltDistance; }

	/** World direction the flame is pushed (away from the way she is coming), scaled 0-1 by how close she is. */
	UFUNCTION(BlueprintPure, Category = "Candle|Flame")
	FVector GetGutterLean() const { return AwayFromKeeper * KeeperGutter; }

	/** 0-1 while a gust is taking the flame (the moment before it goes out); 0 otherwise. */
	UFUNCTION(BlueprintPure, Category = "Candle|Flame")
	float GetGustAmount() const;

	/** How far a walker at From feels from To (cm), by navmesh walk or else through the walls. OutAway: the way the
	 *  air she pushes ahead of her arrives at To (flat). OutToward: her first step toward To. */
	float MeasureKeeperDistance(const FVector& From, const FVector& To, AController* Walker, FVector* OutAway = nullptr, FVector* OutToward = nullptr) const;

	/** A protected moment, a scripted beat, a capture or a disguise: no gust may take the flame. */
	bool IsGustHeld() const;

	/** Within this felt distance (cm), and while she is coming, a gust can blow the open flame out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Candle|Gust")
	float GustStartDistance = 320.f;

	/** At or inside this felt distance the chance of a gust is at its height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Candle|Gust")
	float GustSureDistance = 110.f;

	/** Chance of a gust per second at its height. Once per encounter at most. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Candle|Gust")
	float GustRatePerSecond = .9f;

	/** How long the gust lays the flame over before it goes (s). Cupping inside it saves the flame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Candle|Gust")
	float GustSeconds = .45f;

	/** The push of air as a gust starts to take the flame (/Game/FinalPass/Candle/S_Candle_Gust), before the puff. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Candle|Gust")
	float GustVolume = .55f;

	/** A struck match's flare at its height, as a multiple of the candle's own light. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Candle|Gust")
	float MatchFlareBoost = 2.2f;

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
	UPROPERTY(Transient) TObjectPtr<USoundBase> GustSound;
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
	float LayDownStarted = -100.f;
	float LayDownSeconds = 0.f;
	bool bExternalPresentation = false;
	void UpdateKeeperDistance(float DeltaTime);
	void UpdateGust(float DeltaTime);
	FTallowCandleGust Gust;
	float GustRemaining = 0.f;
	float FeltDistance = 100000.f;
	/** The Keeper's own share of the gutter (sprinting and draughts gutter too, but do not lean the flame from her). */
	float KeeperGutter = 0.f;
	float KeeperClosing = 0.f;
	float NextKeeperQuery = 0.f;
	float NextKeeperSearch = 0.f;
	FVector AwayFromKeeper = FVector::ForwardVector;
	FVector AwayTarget = FVector::ForwardVector;
	FVector TowardNell = FVector::ForwardVector;
	bool bFlareShown = false;
};
