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
