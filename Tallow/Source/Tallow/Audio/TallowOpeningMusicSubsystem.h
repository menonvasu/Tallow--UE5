#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UObject/SoftObjectPtr.h"
#include "TallowOpeningMusicSubsystem.generated.h"

class UAudioComponent;
class USoundBase;

/** My waltz for the opening (28 Oct: "this music can play from the main menu into the start of the game, to the
 *  point of Mim saying the first lines"). It starts on the menu, carries on under the story stills and through the
 *  level load (the sound belongs to the game instance, not to a map), and lets go when Mim says "Hiiii, Nell." in the
 *  chair, or a little after the house has begun if that line never comes. It loops for as long as it is wanted. */
UCLASS()
class TALLOW_API UTallowOpeningMusicSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	/** The music for this game instance (for scripts and tests: Python has no subsystem getter here). */
	UFUNCTION(BlueprintPure, Category = "Music", meta = (WorldContext = "WorldContext"))
	static UTallowOpeningMusicSubsystem* GetOpeningMusic(const UObject* WorldContext)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		return World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UTallowOpeningMusicSubsystem>() : nullptr;
	}

	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual ETickableTickType GetTickableTickType() const override { return IsTemplate() ? ETickableTickType::Never : ETickableTickType::Always; }

	/** The menu opens: start the waltz, or keep it if it is still playing from before. */
	UFUNCTION(BlueprintCallable, Category = "Music") void PlayFromMenu(USoundBase* Sound, float FadeInSeconds = 3.f);
	/** The menu's flash: the waltz all but stops at once, holds there, then creeps back in. */
	UFUNCTION(BlueprintCallable, Category = "Music") void Duck(float HoldSeconds, float ReturnSeconds);
	/** Under the stills' narration, and lower still under Nell's own lines. The director reports both every frame. */
	void SetUnderVoice(bool bNarration, bool bNell) { bUnderNarration = bNarration; bUnderNell = bNell; }
	/** Play has begun in the house. Waking in the chair, the waltz waits for Mim's first line (or gives up after a
	 *  while); continuing from a later checkpoint, there is no first line to wait for, so it goes now. */
	void NoteHouseBegun(bool bInChair);
	/** Mim's first line: let the waltz go. */
	UFUNCTION(BlueprintCallable, Category = "Music") void Finish(float FadeSeconds = 5.f);

	UFUNCTION(BlueprintPure, Category = "Music") bool IsPlaying() const;
	/** The level the mix is asking for right now (before the engine's own fades), for review. The flash duck is
	 *  squared so its return eases in rather than jumping. */
	UFUNCTION(BlueprintPure, Category = "Music|Review") float GetMixLevel() const { return Level * DuckLevel * DuckLevel; }
	UFUNCTION(BlueprintPure, Category = "Music|Review") bool IsFinishing() const { return bFinishing; }
	UFUNCTION(BlueprintPure, Category = "Music|Review") bool HasStarted() const { return bStarted; }

	/** Levels relative to the asset's own volume (the asset carries the measured mastering gain). */
	float MenuLevel = 1.f;
	float NarrationLevel = .22f;
	float NellLevel = .18f;
	float HouseLevel = .7f;
	float FlashLevel = .04f;
	/** If Mim's first line never plays, the waltz goes this long after the house begins. */
	float HouseCapSeconds = 30.f;
	/** The menu map plays this on its own when nothing else has started the waltz shortly after it begins, so the
	 *  music does not depend on the menu scene being placed. A menu scene still starts it with its own sound. */
	TSoftObjectPtr<USoundBase> DefaultWaltz = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/FinalPass/Menu/Audio/MUS_BrokenDollsWaltz.MUS_BrokenDollsWaltz")));
	float DefaultFadeInSeconds = 3.f;
	float MenuStartDelay = .2f;

private:
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Music;
	UPROPERTY(Transient) TObjectPtr<USoundBase> CurrentSound;
	TWeakObjectPtr<UWorld> MenuWorld;
	float MenuSeconds = 0.f;
	bool bMenuChecked = false;
	float Level = 1.f;
	float DuckLevel = 1.f;
	float DuckHold = 0.f;
	float DuckReturn = 1.f;
	float HouseSeconds = -1.f;
	float FinishedFor = 0.f;
	bool bUnderNarration = false;
	bool bUnderNell = false;
	bool bFinishing = false;
	bool bStarted = false;
};
