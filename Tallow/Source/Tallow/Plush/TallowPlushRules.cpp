#include "Plush/TallowPlushRules.h"

float TallowPlushRules::DistanceAlongPath(const FVector& Point, const TArray<FVector>& Path, float& OutDistance)
{
    float Along = 0.f, BestAlong = 0.f; OutDistance = MAX_flt;
    for (int32 I = 1; I < Path.Num(); ++I)
    {
        const FVector Segment = Path[I] - Path[I - 1];
        const float Length = Segment.Size();
        const float T = Length > KINDA_SMALL_NUMBER ? FMath::Clamp(FVector::DotProduct(Point - Path[I - 1], Segment) / (Length * Length), 0.f, 1.f) : 0.f;
        const float Distance = FVector::Dist(Point, Path[I - 1] + Segment * T);
        if (Distance < OutDistance) { OutDistance = Distance; BestAlong = Along + Length * T; }
        Along += Length;
    }
    return BestAlong;
}
bool TallowPlushRules::IsSafe(const FTallowPerchCandidate& P, const FTallowPerchContext& C)
{
    if (!P.bUsable || P.bSeen || FVector::Dist(P.Position, C.Player) < FMath::Max(120.f, C.Rules.MinPlayerDistanceCm)) return false;
    for (const FVector& Door : C.Doorways) if (FMath::Abs(P.Position.Z - Door.Z) < 180.f && FVector::Dist2D(P.Position, Door) < 120.f) return false;
    float Distance; DistanceAlongPath(P.Position, C.Path, Distance);
    return Distance >= FMath::Max(0.f, C.Rules.MinPathDistanceCm);
}
float TallowPlushRules::Score(const FTallowPerchCandidate& P, const FTallowPerchContext& C)
{
    if (!IsSafe(P, C)) return -MAX_flt;
    float Distance; const float Ahead = DistanceAlongPath(P.Position, C.Path, Distance);
    const bool bAhead = Ahead >= C.Rules.GuideAheadMinCm && Ahead <= C.Rules.GuideAheadMaxCm;
    return -(P.StoryPathDistance >= 0.f ? P.StoryPathDistance : FVector::Dist(P.Position, C.Story)) + (bAhead ? 2000.f : 0.f) + (Distance <= 200.f ? 1000.f : 0.f) + (P.bVisibleFromEntry ? 500.f : 0.f);
}
int32 TallowPlushRules::Choose(const TArray<FTallowPerchCandidate>& Perches, const FTallowPerchContext& Context)
{
    int32 Best = INDEX_NONE; float BestScore = -MAX_flt;
    for (int32 I = 0; I < Perches.Num(); ++I)
    {
        const float Value = Score(Perches[I], Context);
        if (Value > BestScore) { Best = I; BestScore = Value; }
    }
    return Best;
}
