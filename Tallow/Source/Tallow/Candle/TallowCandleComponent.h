#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TallowCandleComponent.generated.h"

class UCharacterMovementComponent;
class UPointLightComponent;
struct FTimerHandle;

UENUM(BlueprintType)
enum class ECandleState : uint8
{
	Out,
	Lit,
	Cupped
};

// delegates for events
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCandleStateChanged, ECandleState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRelightStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCandleGutteringChanged, bool, bIsGuttering);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class TALLOW_API UTallowCandleComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	/** Sets default values for this component's properties */
	UTallowCandleComponent();

	/** Set candle state*/
	UFUNCTION(BlueprintCallable, Category = "Candle")
	void SetCandleState(ECandleState NewState);

	/** Start and finish relighting */
	UFUNCTION(BlueprintCallable, Category = "Candle")
	void StartRelighting();

	UFUNCTION()
	void FinishRelighting();

	/** Start and finish sprints */
	UFUNCTION(BlueprintCallable, Category = "Candle")
	void HandleSprintStarted();

	UFUNCTION()
	void HandleSprintEnded();
	
	/** Get current state */
	UFUNCTION(BlueprintPure, Category = "Candle")
	ECandleState GetCandleState() const { return CurrentState; }

	/** Get if currently guttering */
	UFUNCTION(BlueprintPure, Category = "Candle")
	bool IsGuttering() const
	{
		return GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(SprintExtinguishTimerHandle);
	}

	/** Exposed delegate properties */
	UPROPERTY(BlueprintAssignable, Category = "Candle|Events")
	FOnCandleStateChanged OnCandleStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Candle|Events")
	FOnRelightStarted OnRelightStarted;

	UPROPERTY(BlueprintAssignable, Category = "Candle|Events")
	FOnCandleGutteringChanged OnCandleGutteringChanged;

protected:
	/** Current Candle state variable */
	ECandleState CurrentState = ECandleState::Out;

	/** Pointers to character movement and light*/
	UPROPERTY(VisibleAnywhere, Category = "Candle|Components")
	TObjectPtr<UCharacterMovementComponent> CachedMovementComponent;

	UPROPERTY(VisibleAnywhere, Category = "Candle|Components")
	TObjectPtr<UPointLightComponent> CachedLightComponent;

	UPROPERTY(VisibleAnywhere, Category = "Candle|Components")
	float DefaultWalkSpeed;

	/** Tunables */
	UPROPERTY(EditDefaultsOnly, Category = "Candle|Tuning")
	float LitLightRadius = 800.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Candle|Tuning")
	float CuppedLightRadius = 250.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Candle|Tuning")
	float CuppedSpeedMultiplier = 0.65f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Candle|Tuning")
	float RelightNoiseLoudness = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Candle|Tuning")
	int32 MatchCount = 3;

	UPROPERTY(EditDefaultsOnly, Category = "Candle|Tuning")
	float RelightDuration = 1.2f;

	FTimerHandle RelightTimerHandle;

	/** Handles candle extinguish on sprint */
	UFUNCTION()
	void HandleSprintExtinguishTimerExpired();

	FTimerHandle SprintExtinguishTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Candle|Tuning")
	float SprintExtinguishGracePeriod = 1.5f;

	/** Called when game starts */
	virtual void BeginPlay() override;



};
