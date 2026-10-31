#pragma once
#include "CoreMinimal.h"
#include "Audio/TallowSoundscapeComponent.h"

/** Deliberate foreground priorities; intensity comes from context and readable tells. */
struct TALLOW_API FTallowAudioProfile
{
	float Bed=1.f, Dark=0.f, Bass=0.f, Horror=0.f;
	float PlayerVoice=1.f, PlayerFoley=1.f, Mim=1.f, Narration=1.f, Distractions=1.f;
	bool bAllowDistractions=true;
	static FTallowAudioProfile For(ETallowTension State, bool bListening, bool bReading, bool bLate);
};

/** 30 Oct playtest: "remove all random noises Nell makes. The lines are fine and the breathing noises". Her gasps,
 *  efforts, the strain whimper and the out-of-breath pant (two voiced exhales every second after a run, the "eh eh") stay
 *  off unless tallow.NellVocalFx is 1. Every Nell sound that is not a line is logged as TALLOW_VOICE, played or muted. */
namespace TallowNellVoice
{
	TALLOW_API bool VocalFxOn();
	TALLOW_API bool AllowVocalFx(const TCHAR* Cue);
	TALLOW_API void NoteBreath(const TCHAR* Cue);
}

