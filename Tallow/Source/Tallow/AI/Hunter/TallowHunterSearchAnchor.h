#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TallowHunterSearchAnchor.generated.h"

UENUM(BlueprintType)
enum class ETallowHunterAnchorType : uint8
{
    Threshold, Cover, High, Ambush, Squeeze
};

/** Authored places to inspect or wait. Contains no player occupancy information. */
UCLASS()
class TALLOW_API ATallowHunterSearchAnchor : public AActor
{
    GENERATED_BODY()
public:
    ATallowHunterSearchAnchor();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hunter")
    ETallowHunterAnchorType AnchorType = ETallowHunterAnchorType::Threshold;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hunter")
    FName RoomId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hunter")
    FVector FocusOffset = FVector(150.f, 0.f, 80.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hunter")
    float Priority = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hunter")
    bool bEnabled = true;
    UFUNCTION(BlueprintPure, Category="Hunter")
    FVector GetFocusLocation() const { return GetActorTransform().TransformPosition(FocusOffset); }
};
