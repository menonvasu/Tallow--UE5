#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TallowStorySubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTallowLoreRead, FName /*Key*/);

/** Stable save identifiers for the two 1974 endings. */
UENUM(BlueprintType)
enum class ETallowEnding : uint8
{
	GoodGirl,       // A: Another Child Found Safe
	SaidNo          // B: The Finished Child
};

/** What the story remembers across the map change into the ending: which ending this run earned, and which lore
 *  papers were read (the constable's unsent letter unlocks his confession after either ending). */
UCLASS()
class TALLOW_API UTallowStorySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** The story for this game instance (for scripts and tests: Python has no subsystem getter here). */
	UFUNCTION(BlueprintPure, Category = "Story", meta = (WorldContext = "WorldContext"))
	static UTallowStorySubsystem* GetStory(const UObject* WorldContext)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		return World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UTallowStorySubsystem>() : nullptr;
	}

	/** A new run from the menu. */
	UFUNCTION(BlueprintCallable, Category = "Story") void BeginRun() { Ending = ETallowEnding::GoodGirl; Read.Reset(); }

	/** A capture restarts the house: the candle is back in Nell's hands, so the choice is undone. Papers stay read. */
	UFUNCTION(BlueprintCallable, Category = "Story") void ResetHouse() { Ending = ETallowEnding::GoodGirl; }

	UFUNCTION(BlueprintCallable, Category = "Story") void GiveCandleBack() { Ending = ETallowEnding::SaidNo; }
	UFUNCTION(BlueprintPure, Category = "Story") ETallowEnding GetEnding() const { return Ending; }

	/** Lore pickups carry an actor tag Tallow.Lore.<key>. */
	UFUNCTION(BlueprintCallable, Category = "Story") void NoteRead(FName Key) { bool bAgain = false; Read.Add(Key, &bAgain); if (!bAgain) OnLoreRead.Broadcast(Key); }
	UFUNCTION(BlueprintPure, Category = "Story") bool HasRead(FName Key) const { return Read.Contains(Key); }
	TArray<FName> GetReadKeys() const { return Read.Array(); }
	void RestoreRead(const TArray<FName>& Keys) { Read.Reset(); Read.Append(Keys); }
	/** The first time each paper is read (the portrait makes Mim speak). */
	FOnTallowLoreRead OnLoreRead;

	/** Keep the player's reading preference across pages and papers for this session. */
	UFUNCTION(BlueprintCallable, Category="Story|Audio")
	void SetDocumentNarrationMuted(bool bMuted) { bDocumentNarrationMuted = bMuted; }
	UFUNCTION(BlueprintPure, Category="Story|Audio")
	bool IsDocumentNarrationMuted() const { return bDocumentNarrationMuted; }

private:
	ETallowEnding Ending = ETallowEnding::GoodGirl;
	TSet<FName> Read;
	bool bDocumentNarrationMuted = false;
};
