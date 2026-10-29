#include "House/TallowRoomSearchTracker.h"

bool FTallowRoomSearchTracker::CanSee(const FTallowSearchPoint& Point, const FVector& Eye, const FVector& Forward, float Radius, float ConeDegrees)
{
    const FVector To = Point.Position - Eye;
    return Point.bVisible && To.SizeSquared() <= FMath::Square(FMath::Max(0.f, Radius))
        && (To.IsNearlyZero() || FVector::DotProduct(To.GetSafeNormal(), Forward.GetSafeNormal()) >= FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(ConeDegrees, 0.f, 180.f))));
}
void FTallowRoomSearchTracker::Step(FName Room, const FVector& Eye, const FVector& Forward, bool bLit, float Dt, const TArray<FTallowSearchPoint>& Points)
{
    if (Room.IsNone() || Dt <= 0.f) return;
    auto& S = Rooms.FindOrAdd(Room);
    S.Dwell += Dt;
    S.bVisited |= S.Dwell >= 1.f - KINDA_SMALL_NUMBER;
    const float Yaw = FMath::Fmod(Forward.Rotation().Yaw + 360.f, 360.f);
    S.YawBins |= 1u << FMath::Clamp(FMath::FloorToInt(Yaw / 30.f), 0, 11);
    const float Radius = bLit ? Rules.SeenRadiusCm : Rules.DarkSeenRadiusCm;
    for (const auto& P : Points) if (CanSee(P, Eye, Forward, Radius, Rules.ViewConeDegrees)) S.Seen.Add(P.Id);
    float Coverage = 0.f;
    float DwellGoal = Rules.MinDwellSeconds;
    if (Points.IsEmpty())
    {
        int32 Bins = 0;
        for (int32 I = 0; I < 12; ++I) Bins += (S.YawBins >> I) & 1;
        Coverage = float(Bins) * 30.f / FMath::Max(30.f, Rules.EmptyRoomYawCoverageDegrees);
        DwellGoal = Rules.EmptyRoomDwellSeconds;
    }
    else
    {
        int32 Seen = 0;
        for (const auto& P : Points) Seen += S.Seen.Contains(P.Id) ? 1 : 0;
        Coverage = float(Seen) / FMath::Max(1.f, FMath::CeilToFloat(Points.Num() * FMath::Clamp(Rules.RequiredFraction, 0.f, 1.f)));
    }
    S.Progress = S.bSearched ? 1.f : FMath::Clamp(FMath::Min(S.Dwell / FMath::Max(.1f, DwellGoal), Coverage), 0.f, 1.f);
    S.bSearched |= S.Progress >= 1.f - KINDA_SMALL_NUMBER;
}
void FTallowRoomSearchTracker::ResetRound()
{
    for (auto& Pair : Rooms) { const bool bVisited = Pair.Value.bVisited; Pair.Value = {}; Pair.Value.bVisited = bVisited; }
}
void FTallowRoomSearchTracker::RestoreVisited(const TArray<FName>& Visited)
{
    for (FName Room : Visited) if (!Room.IsNone()) Rooms.FindOrAdd(Room).bVisited = true;
}
TArray<FName> FTallowRoomSearchTracker::GetVisited() const
{
    TArray<FName> Result;
    for (const auto& Pair : Rooms) if (Pair.Value.bVisited) Result.Add(Pair.Key);
    Result.Sort(FNameLexicalLess());
    return Result;
}
const FTallowSearchProgress& FTallowRoomSearchTracker::Get(FName Room) const
{
    if (const auto* S = Rooms.Find(Room)) return *S;
    static const FTallowSearchProgress Empty;
    return Empty;
}
