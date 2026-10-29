#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TallowSubtitles.generated.h"

/** Who is speaking: the colour of the line, never a name. The hidden voice is the one Nell is not meant to be able to place. */
enum class ETallowSubtitleVoice : uint8 { Told, Nell, Mim, Hidden };

/** One caption of a spoken line: shown from Start to End seconds into the sound. */
struct FTallowSubtitleCue
{
	FString Text;
	float Start = 0.f, End = 0.f;
};

namespace TallowSubtitles
{
	/** Subtitles on or off (Settings), saved with the other settings. On unless she has turned them off. */
	TALLOW_API bool Enabled();
	TALLOW_API void SetEnabled(bool bEnabled);
	/** The words of a sound asset (VO_Read_register_01, V_Mim_Chase_mim_chase_01...), or null when none are on record.
	 *  Sounds with no words in them (breaths, giggles) have none and are never captioned. */
	TALLOW_API const FString* TextFor(FName Asset);
	/** The sound a journal page is read from. */
	TALLOW_API FName ReadingAsset(FName Document, int32 Page);
	/** Whole sentences packed into captions of a line or two, timed across Duration by their length. */
	TALLOW_API TArray<FTallowSubtitleCue> Split(const FString& Text, float Duration);
	TALLOW_API const FTallowSubtitleCue* CueAt(const TArray<FTallowSubtitleCue>& Cues, float Position);

	/** The captions sit in a strip along the bottom edge, under Nell's notes (centred at .815 of the height, bottom edge
	 *  no lower than 125 units up at 1080) and the lesson line (92 up); both in 1080p layout units. */
	constexpr float BottomMargin = 16.f;
	constexpr float StripHeight = 72.f;
	constexpr float NoteClearance = 125.f;
	constexpr float LessonClearance = 92.f;
	constexpr int32 MaxCaptionCharacters = 84;
}

/** The words of whatever is being said, low in the middle: one caption at a time per voice, at most two lines of each.
 *  Sources start a track when a line begins (a self-timed one for a sound that just plays, or a clock they set by hand
 *  for a recording that can be paused) and end it when the line does. */
UCLASS()
class TALLOW_API UTallowSubtitleSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UTallowSubtitleSubsystem* Get(const UObject* WorldContext);

	/** A line has begun. Returns the track, or 0 when the sound has no words. bSelfTimed: the subsystem runs its clock. */
	int32 Begin(FName Asset, float Duration, ETallowSubtitleVoice Voice, bool bSelfTimed = true);
	/** For a track the source times itself: where the sound is now, and whether it is being heard right now. */
	void SetPosition(int32 Track, float Position, bool bAudible);
	void End(int32 Track);
	/** The hidden voice's words as the corrupted voice sets them (it times its own lines); empty clears. */
	void SetHidden(const FString& Text);

	/** What is on screen now, one entry per voice that has a caption: empty with subtitles off. */
	TArray<TPair<ETallowSubtitleVoice, FString>> Visible() const;
	int32 NumTracks() const { return Tracks.Num(); }

	virtual void Initialize(FSubsystemCollectionBase& Collection) override { Super::Initialize(Collection); bLive = true; }
	virtual void Tick(float DeltaTime) override;
	/** The self-timed lines run on: Tick does this; checks without a world call it by hand. */
	void Advance(float DeltaTime);
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UTallowSubtitleSubsystem, STATGROUP_Tickables); }
	virtual void Deinitialize() override;

private:
	struct FTrack
	{
		int32 Id = 0;
		ETallowSubtitleVoice Voice = ETallowSubtitleVoice::Told;
		TArray<FTallowSubtitleCue> Cues;
		float Position = 0.f, Duration = 0.f;
		bool bSelfTimed = true, bAudible = true;
	};
	void EnsureWidget();

	TArray<FTrack> Tracks;
	int32 NextId = 1;
	bool bLive = false;
	FString HiddenText;
	TSharedPtr<class SWidget> Widget;
};
