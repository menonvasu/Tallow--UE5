#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TallowChairSpot.generated.h"

UCLASS()
class TALLOW_API ATallowChairSpot : public AActor
{
	GENERATED_BODY()
public:
	ATallowChairSpot();
	UPROPERTY(EditAnywhere, Category="Chair") FName RoomOverride;   // empty: TallowHouseRooms::RoomAt
	UPROPERTY(EditAnywhere, Category="Chair") bool bEnabled = true;
	UPROPERTY(VisibleAnywhere, Category="Chair") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category="Chair") TObjectPtr<USceneComponent> CandlePlace;
	FName GetRoom() const;
	bool IsUsable() const;   // enabled and passes the runtime check (A)
    static bool ValidatePlacement(UWorld* World, class ATallowChairEscape* Chair, const FTransform& Transform,
        bool bFullCheck, float& OutClearance, FString& OutProblem, FVector* OutMim = nullptr);
};


