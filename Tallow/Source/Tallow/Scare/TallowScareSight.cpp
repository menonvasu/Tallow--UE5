#include "Scare/TallowScareSight.h"

float TallowScareSight::DegreesOff(const FView& View, const FVector& At)
{
    const FVector To = (At - View.Eye).GetSafeNormal();
    return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(View.Forward.GetSafeNormal(), To), -1.f, 1.f)));
}
bool TallowScareSight::Fits(const FView& View, const FSpot& Spot, const FBand& Band)
{
    if (!Spot.bClear || (Band.bSameRoom && (View.Room.IsNone() || Spot.Room != View.Room))) return false;
    const float Distance = FVector::Dist(View.Eye, Spot.At), Degrees = DegreesOff(View, Spot.At);
    return Distance >= Band.MinCm && Distance <= Band.MaxCm && FMath::Abs(Spot.At.Z - View.Eye.Z) <= Band.MaxRise
        && Degrees >= Band.MinDegrees && Degrees <= Band.MaxDegrees;
}
int32 TallowScareSight::Pick(const FView& View, const TArray<FSpot>& Spots, const FBand& Band, FRandomStream& Random)
{
    // In front of her, the middle of the view is where she will catch it; behind her, the more squarely behind the better.
    const bool bBehind = Band.MinDegrees >= 90.f;
    TArray<float> Weights; Weights.Init(0.f, Spots.Num());
    float Total = 0.f;
    for (int32 I = 0; I < Spots.Num(); ++I)
        if (Fits(View, Spots[I], Band))
        {
            const float Off = bBehind ? 180.f - DegreesOff(View, Spots[I].At) : DegreesOff(View, Spots[I].At);
            Weights[I] = 1.f / (1.f + Off / 15.f);
            Total += Weights[I];
        }
    if (Total <= 0.f) return INDEX_NONE;
    float Roll = Random.FRand() * Total;
    for (int32 I = 0; I < Spots.Num(); ++I)
        if (Weights[I] > 0.f && (Roll -= Weights[I]) <= 0.f) return I;
    for (int32 I = Spots.Num() - 1; I >= 0; --I) if (Weights[I] > 0.f) return I;
    return INDEX_NONE;
}
