#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Presentation/TallowJournalVoices.h"
#include "Noise/TallowNoiseTypes.h"
namespace TallowPaper { enum class EMark : uint8; }

#include "TallowJournalWidget.generated.h"

/** The scribbled notebook's pages, read one at a time. It owns input while open; the world keeps running. */
UCLASS()
class TALLOW_API UTallowJournalWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTallowJournalWidget(const FObjectInitializer& ObjectInitializer);

	/** bInPrinted: set in type (a newspaper, a poster, the register) instead of a child's hand. */
	void Open(APlayerController* InController, const TArray<FText>& InPages, bool bInPrinted = false, FName InDocumentKey = NAME_None);
	UFUNCTION(BlueprintCallable, Category="Journal") void Close();
	/** Put the paper away while its voice continues; skip explicitly stops it. */
	UFUNCTION(BlueprintCallable, Category="Journal|Audio") void MinimizeReading();
	UFUNCTION(BlueprintCallable, Category="Journal|Audio") void SkipReading();
	UFUNCTION(BlueprintPure, Category="Journal|Audio") bool IsCompact() const { return bOpen&&bCompact; }
	void ConfigureDocument(const TArray<FText>& InPages, bool bInPrinted = false, FName InDocumentKey = NAME_None);
	bool IsOpen() const { return bOpen; }
	void GoToPage(int32 InPage);
	/** Headless checks have no painted Slate, so nothing ticks the widget: drive it by hand. */
	void TickForTests(float DeltaTime) { NativeTick(FGeometry(), DeltaTime); }
	UFUNCTION(BlueprintCallable, Category="Journal|Audio") void PauseReadingForImportantSound(float Seconds=2.f);
	UFUNCTION(BlueprintCallable, Category="Journal|Audio") void ToggleReading();
	UFUNCTION(BlueprintCallable, Category="Journal|Audio") void ReplayReading();
	UFUNCTION(BlueprintPure, Category="Journal|Audio") bool HasReading() const {return Playback.bAvailable;}
	UFUNCTION(BlueprintPure, Category="Journal|Audio") bool IsReading() const {return Playback.IsRunning();}
	UFUNCTION(BlueprintCallable, Category="Journal|Audio") void SetNarrationMuted(bool bMuted);
	UFUNCTION(BlueprintPure, Category="Journal|Audio") bool IsNarrationMuted() const { return bNarrationMuted; }
	UFUNCTION(BlueprintCallable, Category="Journal|Audio") void ReplayNarration();
	UFUNCTION(BlueprintPure, Category="Journal") FName GetDocumentKey() const { return DocumentKey; }
	UFUNCTION(BlueprintPure, Category="Journal") int32 GetPageIndex() const { return Page; }
	UFUNCTION(BlueprintPure, Category="Journal|Audio") float GetReadingPosition() const { return Playback.Position; }
#if WITH_EDITOR
	/** Visual fixture only: no sound component is created and no world/input is changed. */
	void ConfigureReadingReview(FName Document, bool bMinimized, bool bPaused=false, bool bHeld=false, FName SpeakerOverride=NAME_None);
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual void NativeDestruct() override;

private:
	void Turn(int32 Delta);
	FText GetPageText() const;
	FText GetFooterText() const;
	FText ReadingStatus() const;
	FText ReadingAction() const;
	FText ReadingTime() const;
	FName PageIllustration() const;
	/** A key hint for the reader: 0 back a page, 1 put away, 2 next, 3 play or pause, 4 stop the tape. */
	FString KeyHint(int32 Which) const;
	/** The speaker's engraved mark: the reel for a recording, else the paper's own kind. */
	TallowPaper::EMark SpeakerMark() const;
	UFUNCTION() void OnImportantNoise(const FTallowNoiseEvent& Noise);
	UFUNCTION() void ReadingFinished();
	UFUNCTION() void ReadingPercent(const class USoundWave* Wave, float Percent);
	void PlayNarration();
	void StopNarration();
	void TakeReadingInput();
	void ReleaseReadingInput();

	TArray<FText> Pages;
	int32 Page = 0;
	bool bOpen = false;
	bool bCompact = false, bHoldingInput = false;
	bool bHaveAudioPercent = false;
	bool bPrinted = false;
	bool bHadCursor = false;
	bool bNarrationMuted = false;
	bool bNarrationDucked = false;
	float SeenAtOpen = -100.f;
	FName DocumentKey = NAME_None;
	TWeakObjectPtr<APlayerController> Controller;
	FName SpeakerKey;
	FTallowReadingPlayback Playback;
	int32 CorruptSession = 0;
	TWeakObjectPtr<class ATallowCreature> Keeper;
	UPROPERTY(Transient) TObjectPtr<class UAudioComponent> Narration;
	TWeakObjectPtr<class UTallowSoundscapeComponent> AudioFocus;
	UPROPERTY(Transient) TObjectPtr<class UFontFace> HandFace;
	UPROPERTY(Transient) TObjectPtr<class UFontFace> PrintFace;
};
