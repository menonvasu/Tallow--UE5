#include "Audio/TallowAudioPolicy.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarNellVocalFx(TEXT("tallow.NellVocalFx"), 0,
		TEXT("1 brings back Nell's gasps, efforts, whimper and pant (off since the 30 Oct playtest; her lines and breathing always play)."));
}

bool TallowNellVoice::VocalFxOn() { return CVarNellVocalFx.GetValueOnGameThread() != 0; }

bool TallowNellVoice::AllowVocalFx(const TCHAR* Cue)
{
	const bool bAllow = VocalFxOn();
	UE_LOG(LogTemp, Display, TEXT("TALLOW_VOICE Nell%s %s"), bAllow ? TEXT("Fx") : TEXT("Muted"), Cue);
	return bAllow;
}

void TallowNellVoice::NoteBreath(const TCHAR* Cue)
{
	UE_LOG(LogTemp, Display, TEXT("TALLOW_VOICE NellBreath %s"), Cue);
}

FTallowAudioProfile FTallowAudioProfile::For(ETallowTension State, bool bListening, bool bReading, bool bLate)
{
	FTallowAudioProfile P;
	// The calm house is the loud one (28 Oct). Once Mim is near, the house steps back to where it sat before, so
	// her tells keep their margin, and the weight moves into the drones: sub bass from uneasy on, the dark drone
	// under a chase. Tools/FinalPass/ambience/mix_table.py has the measured levels behind these numbers.
	switch(State)
	{
	case ETallowTension::Uneasy: P.Bed=.67f; P.Dark=.12f; P.Bass=.08f; break;
	case ETallowTension::Hunted: P.Bed=.41f; P.Bass=.18f; P.Narration=0.f; break;
	case ETallowTension::Chase: P.Bed=.28f; P.Dark=.1f; P.Bass=.14f; P.Narration=0.f; break;
	case ETallowTension::Caught:
		P.Bed=.05f; P.PlayerVoice=P.PlayerFoley=P.Mim=P.Narration=0.f; break;
	case ETallowTension::After: P.Bed=.78f; break;
	default: break;
	}
	P.bAllowDistractions=State==ETallowTension::Calm || State==ETallowTension::Uneasy;
	if(bLate && State==ETallowTension::Uneasy) P.Horror=.09f;
	if(bLate && State==ETallowTension::Hunted) P.Horror=.055f;
	if(bListening && State!=ETallowTension::Caught)
	{
		P.Bed*=.37f; P.Dark*=.37f; P.Bass*=.37f; P.Horror*=.37f;
		P.bAllowDistractions=false;
	}
	if(bReading)
	{
		P.Bed*=.47f; P.Dark*=.65f; P.Bass*=.65f; P.Horror*=.65f;
		P.PlayerVoice*=.3f; P.bAllowDistractions=false;
	}
	P.Distractions=P.bAllowDistractions?1.f:0.f;
	return P;
}
