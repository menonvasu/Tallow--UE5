#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TallowGameMode.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCaught);

UCLASS(abstract)
class ATallowGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATallowGameMode();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category="Tallow|Game")
	void HandlePlayerCaptured();

	void ResetGame();
	void ClearCaptureState();

	UPROPERTY(BlueprintAssignable, Category = "Tallow|Events")
	FOnCaught OnCaught;
	UPROPERTY(BlueprintAssignable, Category="Tallow|Events")
	FOnCaught OnCapturePresented;

	/** Registers an item dropped on the floor at runtime*/
	void RegisterDroppedItem(AActor* DroppedItem);

public:
	bool IsCaptureInProgress() const { return bCaptureInProgress; }

protected:

	/** References to track */
	UPROPERTY(VisibleInstanceOnly, Category = "Tallow|Game")
	TObjectPtr<class ATallowCharacter> Player;

	UPROPERTY(VisibleInstanceOnly, Category = "Tallow|Game")
	TObjectPtr<class ATallowChairEscape> Chair;

	UPROPERTY(VisibleInstanceOnly, Category = "Tallow|Game")
	TObjectPtr<class ATallowCreature> Creature;

	UPROPERTY(VisibleInstanceOnly, Category = "Tallow|Game")
	TObjectPtr<class ATallowCandlePickup> CandlePickup;
	
	UPROPERTY(VisibleInstanceOnly, Category = "Tallow|Game")
	TObjectPtr<class APlayerCameraManager> CameraManager;
	
	UPROPERTY(VisibleInstanceOnly, Category = "Tallow|Game")
	TObjectPtr<class ATallowFuseBox> FuseBox;
	
	UPROPERTY(VisibleInstanceOnly, Category = "Tallow|Game")
	TArray<TObjectPtr<class ATallowFusePickup>> FusePickups;
	
	UPROPERTY(VisibleInstanceOnly, Category = "Tallow|Game")
	TObjectPtr<class ATallowCrowbarPickup> CrowbarPickup;
	
	UPROPERTY(VisibleInstanceOnly, Category = "Tallow|Game")
	TArray<TObjectPtr<class ATallowDoor>> CachedDoors;
	
	UPROPERTY(VisibleInstanceOnly, Category = "Tallow|Game")
	TArray<TObjectPtr<class ATallowMatchbox>> Matchboxes;
	
	/** Registry of runtime drops */
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> RuntimeDroppedItems;
	
	/** Screen fade timer */
	FTimerHandle ResetTimerHandle;
	FTimerHandle CaptureFadeTimerHandle;
	bool bCaptureInProgress = false;
	bool bCaptureLockedLook = false;
	bool bCaptureLockedMovement = false;
	/** The capture's cut left the picture and the sound on black; whatever restarts the run brings them back. */
	bool bCaptureFadeHeld = false;
	uint8 CaptureMovementMode = 0;
	uint8 CaptureCustomMovementMode = 0;
	void BeginCaptureFade();
	void FinishCapturePresentation();
	/** Preserve the authored capture before fading; unconfigured creatures retain the original timing. */
	UPROPERTY(EditDefaultsOnly, Category = "Tallow|Game")
	bool bShowAuthoredCapture = true;

	/** Capture length when no authored clip fits the Keeper (V13's grab is procedural): hands close, mask opens, fade. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture")
	float ProceduralCaptureDuration = 1.9f;
	
	/** Zero is a hard cut (28 Oct): picture and sound go in the same frame, on the strike's hold. */
	UPROPERTY(EditDefaultsOnly, Category = "Tallow|Game")
	float FadeOutTimer = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Tallow|Game")
	float FadeInTimer = 1.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Tallow|Game")
	float BlackoutTimer = 1.2f;
};


