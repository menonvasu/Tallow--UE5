#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Candle/TallowCandleComponent.h"
#include "TallowCandlePresentationActor.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UTallowFlameVisualComponent;

/** Standalone candle for menu/cinematic staging. Contains no interaction or gameplay light. */
UCLASS()
class TALLOW_API ATallowCandlePresentationActor : public AActor
{
	GENERATED_BODY()
public:
	ATallowCandlePresentationActor();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Presentation")
	TObjectPtr<UStaticMeshComponent> Candle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Presentation")
	TObjectPtr<USceneComponent> Wick;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Presentation")
	TObjectPtr<UTallowFlameVisualComponent> Flame;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Presentation")
	ECandleState FlameState = ECandleState::Lit;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category="Presentation", meta=(ClampMin="0", ClampMax="1"))
	float DraftExposure = .75f;
};
