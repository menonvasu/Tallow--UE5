#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Candle/TallowCandleComponent.h"
#include "TallowFlameVisualComponent.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTallowCandleFlameComponent;

/** Opt-in visible flame. Owns only its mesh/MID; never alters gameplay, light, sound or the candle prop. */
UCLASS(ClassGroup=(Tallow), meta=(BlueprintSpawnableComponent))
class TALLOW_API UTallowFlameVisualComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UTallowFlameVisualComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Supply an anchor at the actual wick. Local +Z points up the flame; +X is draft direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flame|Binding")
	TObjectPtr<USceneComponent> WickAnchor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flame|Binding")
	TObjectPtr<UTallowCandleComponent> CandleSource;
	/** Optional read-only proximity signal; auto-discovered on owner in gameplay mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flame|Binding")
	TObjectPtr<UTallowCandleFlameComponent> GutterSource;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flame|Binding")
	bool bStandalone = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flame|Standalone")
	ECandleState StandaloneState = ECandleState::Lit;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flame|Standalone", meta=(ClampMin="0", ClampMax="1"))
	float StandaloneExposure = .75f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flame|Appearance", meta=(ClampMin="0.1", ClampMax="20"))
	float HeightCm = 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flame|Appearance", meta=(ClampMin="0.05", ClampMax="10"))
	float WidthCm = .9f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flame|Appearance")
	float PhaseOffset = 0.f;
	/** Initial projection for a newly created visual. A runtime presentation owner may override its primitive flag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flame|Appearance")
	bool bFirstPersonVisual = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flame|Appearance")
	TObjectPtr<UStaticMesh> FlameMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flame|Appearance")
	TObjectPtr<UMaterialInterface> FlameMaterial;

	/** Safe to call after setting bindings/assets, including when BeginPlay initially had no anchor. */
	UFUNCTION(BlueprintCallable, Category="Flame")
	bool InitializeVisual();

	/** Available after InitializeVisual; lets the presentation owner share first-person projection. */
	UFUNCTION(BlueprintPure, Category="Flame")
	UStaticMeshComponent* GetVisualMesh() const { return Visual; }

private:
	/** 30 Oct: the candle's flame shows in the same frame its state changes, not on the next stepped (12 fps) tick, so its
	 *  light (which the candle switches on in that frame) can never be seen ahead of it. */
	UFUNCTION() void OnCandleState(ECandleState NewState);
	void LogVisibility(bool bVisible) const;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Visual;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Material;
	float Elapsed = 0.f;
	float Exposure = 0.f;
	float IgnitionRemaining = 0.f;
	FVector2D DraughtBend = FVector2D::ZeroVector;
	/** Holding her breath the flame stands still in a still hand (28 Oct); 0 breathing, 1 held. */
	float Calm = 0.f;
	bool bWasVisible = false;
};
