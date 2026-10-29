#pragma once
#include "CoreMinimal.h"

/** Where a scare she is meant to see may happen, as plain geometry (no world), so the rules can be tested directly:
 *  near her, in her room or its doorway, inside her view or just at its edge, with nothing between her eye and it. */
namespace TallowScareSight
{
    struct FView
    {
        FVector Eye = FVector::ZeroVector;
        FVector Forward = FVector::ForwardVector;
        FName Room;
    };
    struct FSpot
    {
        FVector At = FVector::ZeroVector;
        FName Room;            // the room it belongs to, taken a step towards her (a door counts for the room she is in)
        bool bClear = false;   // a line from her eye reaches it: never through a wall
    };
    /** Distance and bearing a spot must sit in. Degrees are measured off the middle of her view, so 0 to 40 is in front
     *  of her and 110 to 180 is behind her. */
    struct FBand
    {
        float MinCm = 120.f, MaxCm = 450.f;
        float MinDegrees = 0.f, MaxDegrees = 40.f;
        float MaxRise = 200.f;
        bool bSameRoom = true;
    };
    TALLOW_API float DegreesOff(const FView& View, const FVector& At);
    TALLOW_API bool Fits(const FView& View, const FSpot& Spot, const FBand& Band);
    /** One of the spots that fit, nearer the middle of her view (or nearer straight behind her) more often; INDEX_NONE if none. */
    TALLOW_API int32 Pick(const FView& View, const TArray<FSpot>& Spots, const FBand& Band, FRandomStream& Random);
}
