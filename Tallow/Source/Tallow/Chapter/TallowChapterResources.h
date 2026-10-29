#pragma once
#include "CoreMinimal.h"
#include "Chapter/TallowChapterTypes.h"
class ATallowCharacter;
struct TALLOW_API FTallowChapterResources
{
    static FTallowResourceSnapshot Capture(const ATallowCharacter* Player);
    static FTallowResourceSnapshot ForNewChapter(const FTallowResourceSnapshot& Carried, const FTallowResourceRules& Rules);
    static void Restore(ATallowCharacter* Player, const FTallowResourceSnapshot& Snapshot);
};
