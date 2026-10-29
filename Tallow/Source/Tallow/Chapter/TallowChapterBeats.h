#pragma once
#include "CoreMinimal.h"
#include "Chapter/TallowChapterTypes.h"

struct TALLOW_API FTallowChapterBeatState
{
    FTallowBeatContext Context;
    double StartedAt = 0.0;
    double FallbackAt = -1.0;
    uint64 RequestedFrame = 0;
    bool bClaimed = false;
    bool bCut = false;
    bool IsActive() const { return Context.Beat != ETallowChapterBeat::None; }
    bool Matches(ETallowChapterBeat Beat, int32 Serial) const { return IsActive() && Context.Beat == Beat && Context.Serial == Serial; }
    bool Claim(ETallowChapterBeat Beat, int32 Serial, uint64 Frame);
    void Reset() { *this = {}; }
};
namespace TallowChapterBeats
{
    TALLOW_API void Fade(const UObject* Context, bool bOut, float Seconds);
}
