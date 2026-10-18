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

	void HandlePlayerCaptured();

	void ResetGame();

	UPROPERTY(BlueprintAssignable, Category = "Tallow|Events")
	FOnCaught OnCaught;

	/** Registers an item dropped on the floor at runtime*/
	void RegisterDroppedItem(AActor* DroppedItem);

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
	
	/** Registry of runtime drops */
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> RuntimeDroppedItems;
	
	/** Screen fade timer */
	FTimerHandle ResetTimerHandle;
	FTimerHandle CaptureFadeTimerHandle;
	bool bCaptureInProgress = false;
	bool bCaptureLockedLook = false;
	void BeginCaptureFade();
	/** Preserve the authored capture before fading; unconfigured creatures retain the original timing. */
	UPROPERTY(EditDefaultsOnly, Category = "Tallow|Game")
	bool bShowAuthoredCapture = true;
	
	UPROPERTY(EditDefaultsOnly, Category = "Tallow|Game")
	float FadeOutTimer = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category = "Tallow|Game")
	float FadeInTimer = 1.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Tallow|Game")
	float BlackoutTimer = 1.0f;
};


