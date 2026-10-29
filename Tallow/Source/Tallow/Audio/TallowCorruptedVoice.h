#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TallowCorruptedVoice.generated.h"

/** Mim's hidden second narration: short lines laid over the story recordings and readings at authored times. */
enum class ETallowCorruptPlacement : uint8 { Under, Seam, Break, After };

struct TALLOW_API FTallowCorruptCue
{
	FName Line, Group, Key;
	float Start = 0.f, Gap = 0.f, Duration = 0.f, Interrupt = 0.f;
	ETallowCorruptPlacement Placement = ETallowCorruptPlacement::Seam;
	int32 MinPlaythrough = 1;
	FString Asset, Subtitle;
};

/** How the recording's own clock is behaving this frame. Held: the narration is frozen but the world is not. */
enum class ETallowCorruptClock : uint8 { Running, Held, Paused, Finished };

struct TALLOW_API FTallowCorruptEvent
{
	enum class EKind : uint8 { Start, End, Cancel };
	EKind Kind = EKind::Start;
	const FTallowCorruptCue* Cue = nullptr;
	int32 Session = 0;
	/** Start: the recording time the line began at (chained lines add their offset). */
	float Time = 0.f;
};

namespace TallowCorrupted
{
	/** Every authored cue (Tools/FinalPass/overlay/gen_cues.py). */
	TALLOW_API const TArray<FTallowCorruptCue>& Cues();
	/** Fixed gap a break adds after its chain so the original resumes on a breath. */
	constexpr float BreakTail = .25f;
	TALLOW_API bool ShowHiddenVoice();
	TALLOW_API void SetShowHiddenVoice(bool bShow);
	/** Lines marked second playthrough wait for an ending to have been seen once. */
	TALLOW_API bool StoryCompleted();
	TALLOW_API void MarkStoryCompleted();
	/** End of the last "after" line of a recording, or 0 (slides hold this long plus a beat). */
	TALLOW_API float AfterEnd(FName Key, bool bSecondPlaythrough);
}

/** Pure scheduling of the cues of every reading in progress. No audio, no world: it is driven by the recording's own
 *  clock so pause, hold, skip and restart need nothing more than the state passed to Advance. */
class TALLOW_API FTallowCorruptScheduler
{
public:
	/** Starts the cues of a recording; an earlier run of the same recording is cancelled (a restart). */
	int32 Begin(FName Key, const TArray<FTallowCorruptCue>& All, float Duration, bool bSecondPlaythrough, TArray<FTallowCorruptEvent>& Out);
	void Advance(int32 Session, float Delta, float Position, ETallowCorruptClock Clock, TArray<FTallowCorruptEvent>& Out);
	/** Sessions whose recording has ended and whose page was put away: only their "after" lines are still owed. */
	void AdvanceDetached(float Delta, bool bPaused, TArray<FTallowCorruptEvent>& Out);
	/** bFinished: the recording ran out, so lines due after it still play. Otherwise everything is cancelled. */
	void Release(int32 Session, bool bFinished, TArray<FTallowCorruptEvent>& Out);
	void CancelAll(TArray<FTallowCorruptEvent>& Out);
	/** Seconds the narration should still be held for the break now playing. */
	float HoldRemaining(int32 Session) const;
	bool IsLive(int32 Session) const { return Find(Session) != nullptr; }
	int32 Num() const { return Sessions.Num(); }
	int32 NumDetached() const;

private:
	struct FMember { const FTallowCorruptCue* Cue = nullptr; float Offset = 0.f; bool bStarted = false, bEnded = false; };
	struct FGroup
	{
		TArray<FMember> Members;
		float Start = 0.f, Hold = 0.f, ChainTime = 0.f, FiredAt = 0.f;
		bool bAfter = false, bActive = false, bDone = false;
	};
	struct FSession { int32 Id = 0; FName Key; float Duration = 0.f, PostEnd = 0.f; bool bDetached = false; TArray<FGroup> Groups; };

	const FSession* Find(int32 Session) const;
	FSession* Find(int32 Session);
	void Step(FSession& S, float Delta, float Narration, TArray<FTallowCorruptEvent>& Out);
	void Cancel(FSession& S, TArray<FTallowCorruptEvent>& Out);
	void Prune();

	TArray<FSession> Sessions;
	int32 NextId = 1;
};

/** Plays the scheduler's cues: 2D on the narration sound class (so it ducks with the story but is not muffled when Nell
 *  holds her breath), at most two at once, never the same line twice, faded out over .15 s when cancelled. */
UCLASS()
class TALLOW_API UTallowCorruptedVoiceSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UTallowCorruptedVoiceSubsystem* Get(const UObject* WorldContext);

	/** A reading or slide voice has started: returns the session to drive, or 0 when it has no cues. */
	int32 Begin(FName Key, float Duration);
	void Advance(int32 Session, float Delta, float Position, ETallowCorruptClock Clock);
	void Release(int32 Session, bool bFinished);
	float HoldRemaining(int32 Session) const { return Scheduler.HoldRemaining(Session); }

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UTallowCorruptedVoiceSubsystem, STATGROUP_Tickables); }
	virtual void Deinitialize() override;

private:
	void Handle(const TArray<FTallowCorruptEvent>& Events);
	void Play(const FTallowCorruptEvent& Event);
	void ShowSubtitles();

	FTallowCorruptScheduler Scheduler;
	struct FPlaying { int32 Session = 0; FName Line; TWeakObjectPtr<class UAudioComponent> Component; FString Subtitle; };
	TArray<FPlaying> Playing;
	bool bPausedAudio = false;
	TSharedPtr<class SWidget> SubtitleWidget;
	FText SubtitleText;
	UPROPERTY(Transient) TObjectPtr<class USoundConcurrency> Concurrency;
	UPROPERTY(Transient) TObjectPtr<class USoundClass> NarrationClass;
};
