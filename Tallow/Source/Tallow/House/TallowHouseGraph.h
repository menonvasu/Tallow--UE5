#pragma once
#include "CoreMinimal.h"
#include "Chapter/TallowChapterTypes.h"
#include "TallowHouseGraph.generated.h"

USTRUCT(BlueprintType)
struct TALLOW_API FTallowHouseEdge
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Id;
	UPROPERTY(BlueprintReadOnly) FName RoomA;
	UPROPERTY(BlueprintReadOnly) FName RoomB;
	UPROPERTY(BlueprintReadOnly) ETallowEdgeKind Kind = ETallowEdgeKind::Opening;
	UPROPERTY(BlueprintReadOnly) FName DoorActor;
	UPROPERTY(BlueprintReadOnly) FName BoardsActor;
	UPROPERTY(BlueprintReadOnly) FName GateActor;
	UPROPERTY(BlueprintReadOnly) FVector Doorway = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) FVector ProbeStart = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) FVector ProbeEnd = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) TArray<ETallowDoorState> AllowedStates;
	UPROPERTY(BlueprintReadOnly) ETallowDoorState DefaultState = ETallowDoorState::Open;
	UPROPERTY(BlueprintReadOnly) bool bRequiresPower = false;
	UPROPERTY(BlueprintReadOnly) bool bPlayerOnly = false;
	bool Joins(FName Room) const { return RoomA == Room || RoomB == Room; }
	FName Other(FName Room) const { return RoomA == Room ? RoomB : RoomA; }
};
USTRUCT(BlueprintType)
struct TALLOW_API FTallowHouseGraph
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) TArray<FTallowHouseEdge> Edges;
	const FTallowHouseEdge* Find(FName EdgeId) const;
	TArray<const FTallowHouseEdge*> EdgesOf(FName Room) const;
	static bool IsRollable(const FTallowHouseEdge& Edge) { return Edge.AllowedStates.Num() > 1; }
};
namespace TallowHouseGraph { TALLOW_API const FTallowHouseGraph& Get(); }


