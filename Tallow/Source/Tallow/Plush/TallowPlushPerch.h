#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TallowPlushPerch.generated.h"

UENUM(BlueprintType) enum class ETallowPerchKind : uint8 { Corner, Furniture, Doorway, High, Floor };

UCLASS()
class TALLOW_API ATallowPlushPerch : public AActor
{
	GENERATED_BODY()
public:
	ATallowPlushPerch();
	UPROPERTY(EditAnywhere, Category="Plush") ETallowPerchKind Kind = ETallowPerchKind::Furniture;
	UPROPERTY(EditAnywhere, Category="Plush") bool bAllowScare = true;
	UPROPERTY(EditAnywhere, Category="Plush") FName RoomOverride;
	FName GetRoom() const;
};


