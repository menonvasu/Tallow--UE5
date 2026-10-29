#include "Audio/TallowOpeningMusicSubsystem.h"

#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

void UTallowOpeningMusicSubsystem::Deinitialize()
{
	if (Music) Music->Stop();
	Music = nullptr;
	Super::Deinitialize();
}

TStatId UTallowOpeningMusicSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTallowOpeningMusicSubsystem, STATGROUP_Tickables);
}

bool UTallowOpeningMusicSubsystem::IsPlaying() const
{
	return Music && Music->IsPlaying();
}

void UTallowOpeningMusicSubsystem::PlayFromMenu(USoundBase* Sound, float FadeInSeconds)
{
	if (!Sound) return;
	// The same waltz already going (with or without an audio device) keeps going.
	if (bStarted && !bFinishing && CurrentSound == Sound)
	{
		HouseSeconds = -1.f; // Back on the menu from the house: it is the menu's music again.
		return;
	}
	if (Music) Music->Stop();
	// Not owned by the menu map: the travel into the house would otherwise stop it.
	Music = UGameplayStatics::CreateSound2D(GetGameInstance(), Sound, 1.f, 1.f, 0.f, nullptr, true, false);
	CurrentSound = Sound;
	bStarted = true;
	UE_LOG(LogTemp, Display, TEXT("TALLOW_MUSIC start %s fade %.1fs"), *GetNameSafe(Sound), FadeInSeconds);
	if (!Music) return; // No audio device (-nosound): the mix state below still runs, so tests can read it.
	Music->bIsUISound = true; // Keeps going under the pause menu and the stills.
	Music->FadeIn(FadeInSeconds, 1.f);
	Level = MenuLevel; DuckLevel = 1.f; DuckHold = 0.f;
	HouseSeconds = -1.f; FinishedFor = 0.f; bFinishing = false;
}

void UTallowOpeningMusicSubsystem::Duck(float HoldSeconds, float ReturnSeconds)
{
	DuckLevel = FlashLevel;
	DuckHold = FMath::Max(HoldSeconds, 0.f);
	DuckReturn = FMath::Max(ReturnSeconds, .1f);
	if (Music) Music->SetVolumeMultiplier(GetMixLevel()); // At once: it lands with the sting, not a frame later.
}

void UTallowOpeningMusicSubsystem::NoteHouseBegun(bool bInChair)
{
	if (!bStarted || bFinishing) return;
	UE_LOG(LogTemp, Display, TEXT("TALLOW_MUSIC house begun in chair=%d"), bInChair);
	if (!bInChair) { Finish(6.f); return; }
	if (HouseSeconds < 0.f) HouseSeconds = 0.f;
}

void UTallowOpeningMusicSubsystem::Finish(float FadeSeconds)
{
	if (bFinishing || !bStarted) return;
	bFinishing = true;
	if (Music) Music->FadeOut(FMath::Max(FadeSeconds, .1f), 0.f);
	UE_LOG(LogTemp, Display, TEXT("TALLOW_MUSIC finish over %.1fs"), FadeSeconds);
}

void UTallowOpeningMusicSubsystem::Tick(float DeltaTime)
{
	// The menu map starts the waltz itself if nothing else has shortly after it begins.
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (World && World->HasBegunPlay() && World->GetMapName().Contains(TEXT("Lvl_MenuPresentation")))
	{
		if (MenuWorld.Get() != World) { MenuWorld = World; MenuSeconds = 0.f; bMenuChecked = false; }
		if (!bMenuChecked && (MenuSeconds += DeltaTime) >= MenuStartDelay)
		{
			bMenuChecked = true;
			if (!bStarted || bFinishing) PlayFromMenu(DefaultWaltz.LoadSynchronous(), DefaultFadeInSeconds);
		}
	}
	if (!bStarted) return;
	if (HouseSeconds >= 0.f && !bFinishing && (HouseSeconds += DeltaTime) > HouseCapSeconds) Finish(8.f);
	if (bFinishing && (FinishedFor += DeltaTime) > 30.f && Music)
	{
		Music->Stop(); // The fade is long over; give the stream back.
		Music = nullptr;
		CurrentSound = nullptr;
	}
	// Voices duck it quickly and release it slowly, so a gap between two lines does not pump the waltz.
	const float Target = bUnderNell ? NellLevel : bUnderNarration ? NarrationLevel : HouseSeconds >= 0.f ? HouseLevel : MenuLevel;
	Level = FMath::FInterpTo(Level, Target, DeltaTime, Target < Level ? 6.f : 1.2f);
	if (DuckHold > 0.f) DuckHold -= DeltaTime;
	else if (DuckLevel < 1.f) DuckLevel = FMath::Min(1.f, DuckLevel + DeltaTime / DuckReturn);
	if (Music) Music->SetVolumeMultiplier(GetMixLevel());
}
