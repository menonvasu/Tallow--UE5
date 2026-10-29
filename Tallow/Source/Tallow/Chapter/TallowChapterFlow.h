#pragma once
#include "CoreMinimal.h"
#include "Chapter/TallowChapterTypes.h"

struct TALLOW_API FTallowChapterFlow
{
    static bool CanTransition(ETallowChapterKind Kind, ETallowChapterPhase From, ETallowChapterPhase To);
    static ETallowChapterPhase OnEvent(ETallowChapterKind Kind, ETallowChapterPhase Current, ETallowChapterEvent Event, bool bPlayerInChairRoom);
    static int32 ChapterSeed(int32 CampaignSeed, int32 Index);
};
