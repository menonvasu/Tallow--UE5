#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TallowSlidesWidget.generated.h"

class UTexture2D;
class USoundBase;
class UAudioComponent;

/** One still of the story: the image, my voice over it (the old constable), and how long it holds. */
USTRUCT()
struct FTallowSlide
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UTexture2D> Image;
	UPROPERTY() TObjectPtr<USoundBase> Voice;
	float Seconds = 4.f;
	/** Names the recording for the hidden second voice (slide:<name>); none when empty. */
	FName CorruptKey;
	/** The still's name (Intro_01, EndB_03...): finds its caption when the plate carries none of its own. */
	FName Name;
};

/** The opening and ending stills (TALLOW-LORE.md), Five Nights at Freddy's style: each fades up, the voice plays,
 *  it holds as long as I spoke over it in the booth, and fades down. Space skips. */
UCLASS()
class TALLOW_API UTallowSlidesWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTallowSlidesWidget(const FObjectInitializer& ObjectInitializer);

	void Play(const TArray<FTallowSlide>& InSlides, TFunction<void()> InOnDone);
	/** Skip to the end (the caller's done step still runs once). */
	void Finish();
	bool IsPlaying() const { return bPlaying; }
	/** The plate for a still: the final pass's caption-free plate if there is one, else the older captioned still. */
	static UTexture2D* LoadPlate(FName Name);
#if WITH_EDITOR
	void ConfigureReview(UTexture2D* Image);
	void ConfigureReview(FName Name);
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;

private:
	void ShowSlide(int32 Index);
	void EndCorrupt(bool bFinished);
	const FSlateBrush* GetBrush() const;
	FLinearColor GetTint() const;
	/** The caption's fade: it follows the picture in, the sub-line a beat behind the headline. */
	float CaptionAlpha(float Delay) const;
	/** True when the shown plate is caption-free, so the words are set here in Fell. */
	bool HasNativeCaption() const;
	TSharedRef<SWidget> MakeCaption();

	UPROPERTY(Transient) TArray<FTallowSlide> Slides;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Voice;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Bed;
	FSlateBrush Brush;
	FSlateBrush Wordmark;
	TFunction<void()> OnDone;
	int32 Current = -1;
	float Elapsed = 0.f;
	bool bPlaying = false;
	bool bVoiceStarted = false;
	int32 CorruptSession = 0;
	int32 SubtitleTrack = 0;
	bool bDucked = false;
};
