#pragma once

#include "CoreMinimal.h"

namespace TallowJournalVoices
{
 TALLOW_API FName Identify(const TArray<FText>& Pages);
 TALLOW_API FName Speaker(FName Document, int32 Page=0);
 TALLOW_API FText Name(FName Speaker);
 /** A combined reading has two existing portraits; it is never a texture name. */
 TALLOW_API FName Portrait(FName Speaker, int32 Index=0);
}

/** Pause never seeks: a priority cue and a manual pause can overlap independently. */
struct TALLOW_API FTallowReadingPlayback
{
 bool bAvailable=false, bUserPaused=false, bFinished=false, bPriorityPaused=false;
 float Position=0, Duration=0;
 double HoldUntil=0;
 void Prepare(bool bHasAudio, float Length, bool bAutoPlay=true);
 void Interrupt(double Now, float Seconds);
 void Advance(float Delta, double Now, bool bWorldPaused=false);
 void Toggle();
 void Replay();
 bool IsRunning() const {return bAvailable&&!bUserPaused&&!bFinished&&!bPriorityPaused;}
};
