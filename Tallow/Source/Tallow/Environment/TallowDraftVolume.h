#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TallowDraftVolume.generated.h"

class UBoxComponent;

UCLASS()
class TALLOW_API ATallowDraftVolume : public AActor
{
	GENERATED_BODY()

public:
	// constructor
	ATallowDraftVolume();

protected:
	
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Draft|Components")
	TObjectPtr<UBoxComponent> TriggerBox;

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool FromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};