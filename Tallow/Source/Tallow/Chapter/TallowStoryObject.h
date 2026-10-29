#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable/IInteractable.h"
#include "TallowStoryObject.generated.h"

/** The chapter's tape machine. "listen" plays the chapter's essential reading through the journal's own reading
 *  (UTallowJournalWidget, tucked away so she keeps moving): the same VO_Read_<Reading>_NN pages, playback and
 *  read:<document>:<page> keys the papers use, so the hidden voice lays its lines over the tape. The reels turn
 *  while it plays; when it ends (or she stops it) the clue is done. While it is the tape she still has to hear it
 *  looks alive (29 Oct: tell it from the lore tapes at a glance): a small warm-red lamp glows on its deck and the
 *  reels creep round. Once played, the lamp goes out and the reels stop. */
UCLASS()
class TALLOW_API ATallowStoryObject : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	ATallowStoryObject();
	UPROPERTY(EditAnywhere, Category="Story") int32 ChapterIndex = 0;
	UPROPERTY(EditAnywhere, Category="Story") FName ClueKey;          // NoteRead key for the papers archive; empty: the reading
	/** Document whose reading is on the tape; empty: the chapter's essential reading. */
	UPROPERTY(EditAnywhere, Category="Story") FName Reading;
	/** Played from the machine instead when the reading has no pages. */
	UPROPERTY(EditAnywhere, Category="Story") TObjectPtr<USoundBase> Recording;
	UPROPERTY(EditAnywhere, Category="Story") float MinListenSeconds = 4.f;
	UPROPERTY(EditAnywhere, Category="Story") float ReelDegreesPerSecond = 160.f;
	UPROPERTY(VisibleAnywhere, Category="Story") TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere, Category="Story") TObjectPtr<UStaticMeshComponent> ReelA;
	UPROPERTY(VisibleAnywhere, Category="Story") TObjectPtr<UStaticMeshComponent> ReelB;
	UPROPERTY(VisibleAnywhere, Category="Story") TObjectPtr<UAudioComponent> Speaker;
	UPROPERTY(VisibleAnywhere, Category="Story") TObjectPtr<USceneComponent> WritingAnchor;
	/** The lamp on the deck, in front of the reels, and the little light it throws. */
	UPROPERTY(VisibleAnywhere, Category="Story") TObjectPtr<UStaticMeshComponent> Indicator;
	UPROPERTY(VisibleAnywhere, Category="Story") TObjectPtr<class UPointLightComponent> IndicatorLight;
	UPROPERTY(EditAnywhere, Category="Story") float IdleReelDegreesPerSecond = 9.f;
	UPROPERTY(EditAnywhere, Category="Story") float IndicatorLumens = .4f;
	FName GetRoom() const;
	void SetAvailable(bool bAvailable);
	void ResetForRound();
	bool IsPlaying() const;
	virtual void OnInteract_Implementation(AActor* Interactor) override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractionPromptText_Implementation(AActor* Interactor) const override;
    virtual void PostInitializeComponents() override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void Tick(float DeltaSeconds) override;
    bool IsAvailable() const { return bAvailable; }
    /** Out, unplayed and the chapter's current objective: the lamp is lit and the reels creep. */
    bool IsWaiting() const;
    float GetIndicatorGlow() const { return Glow; }
    /** register (cellar), portrait (ground floor), drawings (upstairs), labels (final). */
    static FName EssentialReading(int32 Chapter);
    FName GetReading() const;
    /** The reading playing the tape, while it plays. */
    class UTallowJournalWidget* GetReader() const { return Reader; }
private:
    void FinishListening();
    void UpdateIndicator(float DeltaSeconds);
    UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> IndicatorGlow;
    float Glow = 0.f;
    UPROPERTY(Transient) TObjectPtr<class UTallowJournalWidget> Reader;
    double StartedAt = 0.0, MovedAt = 0.0;
    float LastPosition = -1.f;
    bool bAvailable = false;
    bool bPlaying = false;
    bool bFinished = false;
};
