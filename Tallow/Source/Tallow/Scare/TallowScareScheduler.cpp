#include "Scare/TallowScareScheduler.h"

bool FTallowScareScheduler::IsHunt(ETallowChapterPhase Phase) { return Phase == ETallowChapterPhase::ActiveHunt || Phase == ETallowChapterPhase::BatteryAcquired; }
void FTallowScareScheduler::Begin(int32 ChapterIndex, const TArray<FTallowScareGroupDef>& InGroups, const TArray<FName>& Enabled, const FTallowScareRules& InRules, int32 Seed, float Now)
{
    Chapter = ChapterIndex; Rules = InRules; Random.Initialize(Seed); Count = 0; LastGroup = INDEX_NONE; LastFire = Now;
    Groups.Reset(); ReadyAt.Reset();
    for (const auto& Group : InGroups)
        if (!Group.Group.IsNone() && Enabled.Contains(Group.Group) && !Groups.ContainsByPredicate([&](const auto& G) { return G.Group == Group.Group; })) Groups.Add(Group);
    Groups.Sort([](const auto& A, const auto& B) { return A.Group.LexicalLess(B.Group); });
    ReadyAt.Init(Now, Groups.Num());
    Gap = DrawGap(true);
}
float FTallowScareScheduler::DrawGap(bool bFirst)
{
    // A drawn gap keeps the trickle steady without a beat the player can count; later chapters press closer.
    const float Scale = Rules.ChapterGapScale.IsValidIndex(Chapter) ? FMath::Max(.1f, Rules.ChapterGapScale[Chapter]) : 1.f;
    if (bFirst && Rules.FirstGapSeconds.Y > 0.f)
        return Random.FRandRange(FMath::Max(0.f, Rules.FirstGapSeconds.X), FMath::Max(Rules.FirstGapSeconds.X, Rules.FirstGapSeconds.Y)) * Scale;
    const float Min = FMath::Max(0.f, Rules.MinGapSeconds);
    return (Rules.MaxGapSeconds > Min ? Random.FRandRange(Min, Rules.MaxGapSeconds) : Min) * Scale;
}
bool FTallowScareScheduler::CanFire(int32 I, ETallowChapterPhase Phase, float Now, bool bNormal, bool bMajor) const
{
    if (!Groups.IsValidIndex(I) || Count >= Rules.MaxPerChapter || Phase == ETallowChapterPhase::Paused || Phase == ETallowChapterPhase::None) return false;
    const auto& G = Groups[I];
    const bool bPhase = G.Phases.IsEmpty() ? (Phase == ETallowChapterPhase::Exploration || Phase == ETallowChapterPhase::ActiveHunt) : G.Phases.Contains(Phase);
    if (!bPhase || Chapter < G.FirstChapter || Chapter > G.LastChapter || G.Weight <= 0.f || Now < ReadyAt[I]) return false;
    const float Wait = Gap * (IsHunt(Phase) ? FMath::Max(0.f, Rules.HuntGapScale) : 1.f);
    if (Now - LastFire + KINDA_SMALL_NUMBER < Wait || (G.bMajor && !bMajor)) return false;
    return !IsHunt(Phase) || (G.bMajor ? bMajor : bNormal);
}
int32 FTallowScareScheduler::Choose(ETallowChapterPhase Phase, float Now, bool bNormal, bool bMajor, const TArray<int32>* Available)
{
    float Total = 0.f;
    for (int32 I = 0; I < Groups.Num(); ++I)
        if ((!Available || Available->Contains(I)) && CanFire(I, Phase, Now, bNormal, bMajor)) Total += Groups[I].Weight * (I == LastGroup ? .3f : 1.f);
    if (Total <= 0.f) return INDEX_NONE;
    float Roll = Random.FRand() * Total;
    for (int32 I = 0; I < Groups.Num(); ++I)
        if ((!Available || Available->Contains(I)) && CanFire(I, Phase, Now, bNormal, bMajor))
        {
            Roll -= Groups[I].Weight * (I == LastGroup ? .3f : 1.f);
            if (Roll <= 0.f) return I;
        }
    return INDEX_NONE;
}
void FTallowScareScheduler::Commit(int32 I, float Now)
{
    if (!Groups.IsValidIndex(I)) return;
    const auto& G = Groups[I];
    ReadyAt[I] = Now + Random.FRandRange(FMath::Max(0.f, G.CooldownMin), FMath::Max(FMath::Max(0.f, G.CooldownMin), G.CooldownMax));
    LastGroup = I; LastFire = Now; ++Count; Gap = DrawGap(false);
}
