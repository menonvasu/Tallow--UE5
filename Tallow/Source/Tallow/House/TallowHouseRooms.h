#pragma once
#include "CoreMinimal.h"
#include "TallowHouseRooms.generated.h"

USTRUCT(BlueprintType)
struct TALLOW_API FTallowRoomDef
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Id;
	UPROPERTY(BlueprintReadOnly) FString Label;
	UPROPERTY(BlueprintReadOnly) int32 Floor = 0;
	UPROPERTY(BlueprintReadOnly) FVector2D Min = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly) FVector2D Max = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly) bool bChairAllowed = true;
	UPROPERTY(BlueprintReadOnly) bool bBatteryAllowed = true;
	UPROPERTY(BlueprintReadOnly) bool bPowerGated = false;
	bool Contains(const FVector& P) const { return P.X >= Min.X && P.X < Max.X && P.Y >= Min.Y && P.Y < Max.Y; }
	FVector Anchor() const;   // room middle, standing height on its floor
};
namespace TallowHouseRooms
{
	TALLOW_API const TArray<FTallowRoomDef>& All();
	TALLOW_API const FTallowRoomDef* Find(FName Id);
	TALLOW_API int32 FloorAt(float WorldZ);
	TALLOW_API float FloorZ(int32 Floor);
	TALLOW_API FName RoomAt(const FVector& WorldLocation);
}
