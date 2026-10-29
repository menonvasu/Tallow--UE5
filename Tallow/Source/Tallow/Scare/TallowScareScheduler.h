#pragma once
#include "CoreMinimal.h"
#include "Chapter/TallowChapterTypes.h"

/** Selection does not spend a cooldown until an emitter really fires. */
struct TALLOW_API FTallowScareScheduler
{
    void Begin(int32 ChapterIndex, const TArray<FTallowScareGroupDef>& InGroups, const TArray<FName>& EnabledGroups, const FTallowScareRules& InRules, int32 Seed, float Now);
    int32 Choose(ETallowChapterPhase Phase, float Now, bool bAllowsNormal, bool bAllowsMajor, const TArray<int32>* Available = nullptr);
    bool CanFire(int32 Index, ETallowChapterPhase Phase, float Now, bool bAllowsNormal, bool bAllowsMajor) const;
    void Commit(int32 Index, float Now);
    const TArray<FTallowScareGroupDef>& GetGroups() const { return Groups; }
    int32 GetCount() const { return Count; }
    float GetLastFireTime() const { return LastFire; }
    float GetReadyAt(int32 Index) const { return ReadyAt.IsValidIndex(Index) ? ReadyAt[Index] : MAX_flt; }
    float GetGap() const { return Gap; }
    static bool IsHunt(ETallowChapterPhase Phase);
private:
    float DrawGap(bool bFirst);
    TArray<FTallowScareGroupDef> Groups;
    TArray<float> ReadyAt;
    FTallowScareRules Rules;
    FRandomStream Random;
    int32 Chapter = 0, Count = 0, LastGroup = INDEX_NONE;
    float LastFire = 0.f, Gap = 0.f;
};
