#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TallowHouseBoardClearance.generated.h"
class UStaticMeshComponent;

/** Preserve Vasu's plank interactions/reset while keeping pried debris out of
 * character collision and the navigation raster. Attached only in the art house. */
UCLASS()
class TALLOW_API UTallowHouseBoardClearance : public UActorComponent
{
    GENERATED_BODY()
public:
    UTallowHouseBoardClearance();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
private:
    UFUNCTION() void OnBoardBroken(bool bBroken);
    void RestoreBlocking();
    TWeakObjectPtr<UStaticMeshComponent> Board;
    ECollisionResponse OriginalPawnResponse = ECR_Block;
    bool bOriginalNavigation = true, bDebris = false;
};
