#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TallowNellNotes.generated.h"

class ATallowPlayDirector;
class ATallowCharacter;
class ATallowCreature;
class AActor;
class UTallowSoundscapeComponent;
class UTallowNellNoteWidget;

/** Each thing a first-time player has to understand, at the moment it first matters. */
UENUM()
enum class ETallowNote : uint8
{
	Journal, Candle, Guide, Glimpse, Seen, Lost, Close, Listen, HeldPast, Hide, HideClose, Draught, Sprint, Dark,
	NoMatches, Wax, Board, Snap, Omen, Trick, Fuse, LastFuse, Power, Reel, Projector, Code, Open, Retry, Map, Count
};

/** 28 Oct: "I should understand aspects of the game... what certain small triggers, like it looking at you,
 *  mean." No instructions on screen: the first time each thing happens, Nell thinks it, once, in her own hand on a
 *  torn scrap (and says it, where I recorded the line), and it goes into her notes, which the inventory keeps with
 *  her to-do list. Never a second time, never everything: the giggles, the whispers and most of the lies stay
 *  unexplained. Reads the game's real state only; changes nothing. */
UCLASS(ClassGroup=(Tallow))
class TALLOW_API UTallowNellNotesComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UTallowNellNotesComponent();
	void Setup(ATallowPlayDirector* InDirector, ATallowCharacter* InPlayer, ATallowCreature* InKeeper, UTallowSoundscapeComponent* InSoundscape);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** This level's notes, for the inventory and the reader. */
	static UTallowNellNotesComponent* Find(const UObject* WorldContext);
	/** What she has written so far, in order (tests and the capture tools read this). */
	UFUNCTION(BlueprintPure, Category="Notes") TArray<FString> GetWrittenNotes() const;
	bool HasNotes() const { return !Written.IsEmpty(); }
	/** The inventory line: the next thing on her list. */
	FText GetSummary() const;
	/** Her notebook: what she has to do (ticked off as she goes), then what she knows. */
	UFUNCTION(BlueprintPure, Category="Notes") TArray<FText> GetNotebookPages() const;

	/** A bolted door was tried: Nell mutters, but not twice within a few seconds. */
	void NoticeBolted();

	/** Switch the first-time notes off (the notebook stays). */
	UPROPERTY(EditAnywhere, Category="Notes") bool bShowNotes = true;

private:
	void Notice(ETallowNote Note, float Delay = 0.f);
	void Watch(float Now);
	void Present(float Now);
	bool IsSuppressed() const;
	void Compose(ETallowNote Note, FString& Text, FString& Hint, int32& Line) const;
	void Steps(TArray<FString>& Done, FString& Next) const;
	bool HasLearned(ETallowNote Note) const { return (LearnedMask >> uint32(Note)) & 1ull; }
	static bool IsUrgent(ETallowNote Note);
	bool CanSeeKeeper() const;
	float KeeperDistance() const;
	UFUNCTION() void OnCandleAcquired();
	UFUNCTION() void OnPlank(bool bBroken);
	UFUNCTION() void OnSnap();
	UFUNCTION() void OnCaught();
	UFUNCTION() void OnMatches(int32 Count, bool bTriedWithNone);

	TWeakObjectPtr<ATallowPlayDirector> Director;
	TWeakObjectPtr<ATallowCharacter> Player;
	TWeakObjectPtr<ATallowCreature> Keeper;
	TWeakObjectPtr<UTallowSoundscapeComponent> Soundscape;
	TWeakObjectPtr<AActor> Journal, StartMatches, FuseBox;
	UPROPERTY(Transient) TObjectPtr<UTallowNellNoteWidget> Widget;
	TArray<ETallowNote> Written;
	uint64 LearnedMask = 0;
	ETallowNote Pending = ETallowNote::Count, Showing = ETallowNote::Count;
	float PendingFrom = 0.f, PendingUntil = 0.f, ShownUntil = 0.f, NextCheck = 0.f;
	float CandleSince = -1.f, OutSince = -1.f, ThreatAt = -100.f, SeenCall = -100.f, GuideUntil = 0.f;
	int32 HouseEvents = 0, BoltedVariant = 0;
	float BoltedAt = -100.f, FollowAt = -1000.f, BestWalk = 0.f, BestWalkAt = 0.f;
	bool bCaught = false, bWasTied = true, bWasDistraction = false;
};
