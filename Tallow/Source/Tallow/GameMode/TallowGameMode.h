#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TallowGameMode.generated.h"

class ATallowChairEscape;
class ATallowCreature;
class ATallowCandlePickup;
class ATallowCharacter;
class APlayerCameraManager;

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

protected:

	/** References to track */
	UPROPERTY(VisibleInstanceOnly, Category = "Tallow|Game")
	TObjectPtr<ATallowCharacter> Player;

	UPROPERTY(VisibleInstanceOnly, Category = "Tallow|Game")
	TObjectPtr<ATallowChairEscape> Chair;

	UPROPERTY(VisibleInstanceOnly, Category = "Tallow|Game")
	TObjectPtr<ATallowCreature> Creature;

	UPROPERTY(VisibleInstanceOnly, Category = "Tallow|Game")
	TObjectPtr<ATallowCandlePickup> CandlePickup;
	
	UPROPERTY(VisibleInstanceOnly, Category = "Tallow|Game")
	TObjectPtr<APlayerCameraManager> CameraManager;

	/** Screen fade timer */
	FTimerHandle ResetTimerHandle;
	
	UPROPERTY(EditDefaultsOnly, Category = "Tallow|Game")
	float FadeOutTimer = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category = "Tallow|Game")
	float FadeInTimer = 1.2f;
};



