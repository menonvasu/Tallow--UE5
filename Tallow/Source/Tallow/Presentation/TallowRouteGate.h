#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TallowRouteGate.generated.h"
class UStaticMeshComponent;
class ATallowPlayDirector;
/** An authored route gate in the integration map. Keeps physical collision while the hinge opens. */
UCLASS()
class TALLOW_API ATallowRouteGate : public AActor
{
	GENERATED_BODY()
public:
	ATallowRouteGate();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Frame;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> Hinge;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Door;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Route") bool bRequiresPower = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Route") float OpenAngle = 100.f;
private:
	UPROPERTY(Transient) TObjectPtr<ATallowPlayDirector> Director;
	float CurrentAngle = 0.f;
};
