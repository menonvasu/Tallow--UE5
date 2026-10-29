#pragma once
#include "CoreMinimal.h"
#include "Chapter/TallowChapterTypes.h"

struct FTallowPerchCandidate
{
    FVector Position = FVector::ZeroVector;
    bool bSeen = false;
    bool bUsable = true;
    bool bVisibleFromEntry = true;
    float StoryPathDistance = -1.f;
};
struct FTallowPerchContext
{
    FVector Player = FVector::ZeroVector;
    FVector Story = FVector::ZeroVector;
    TArray<FVector> Path;
    TArray<FVector> Doorways;
    FTallowPlushRules Rules;
};
namespace TallowPlushRules
{
    TALLOW_API float DistanceAlongPath(const FVector& Point, const TArray<FVector>& Path, float& OutDistance);
    TALLOW_API bool IsSafe(const FTallowPerchCandidate& Perch, const FTallowPerchContext& Context);
    TALLOW_API float Score(const FTallowPerchCandidate& Perch, const FTallowPerchContext& Context);
    TALLOW_API int32 Choose(const TArray<FTallowPerchCandidate>& Perches, const FTallowPerchContext& Context);
}
