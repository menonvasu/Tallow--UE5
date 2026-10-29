#pragma once
#include "CoreMinimal.h"
#include "Chapter/TallowChapterTypes.h"

struct FTallowSearchPoint
{
    FName Id;
    FVector Position = FVector::ZeroVector;
    bool bVisible = false;
};
struct FTallowSearchProgress
{
    bool bVisited = false;
    bool bSearched = false;
    float Progress = 0.f;
    float Dwell = 0.f;
    uint16 YawBins = 0;
    TSet<FName> Seen;
};

/** Room observations are independent of rendering, actors and the round's permanent discoveries. */
struct TALLOW_API FTallowRoomSearchTracker
{
    void Begin(const FTallowRoomSearchRules& InRules) { Rules = InRules; }
    void Step(FName Room, const FVector& Eye, const FVector& Forward, bool bLit, float Dt, const TArray<FTallowSearchPoint>& Points);
    void ResetRound();
    void RestoreVisited(const TArray<FName>& Rooms);
    TArray<FName> GetVisited() const;
    const FTallowSearchProgress& Get(FName Room) const;
    static bool CanSee(const FTallowSearchPoint& Point, const FVector& Eye, const FVector& Forward, float Radius, float ConeDegrees);
private:
    FTallowRoomSearchRules Rules;
    TMap<FName, FTallowSearchProgress> Rooms;
};
