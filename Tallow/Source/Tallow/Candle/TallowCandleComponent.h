#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "TallowCandleComponent.generated.h"

class UCharacterMovementComponent;
class UPointLightComponent;

UENUM(BlueprintType)
enum class ECandleState : uint8
{
	Out,
	Lit,
	Cupped
};

UENUM(BlueprintType)
enum class ETallowCandleFuelStage : uint8 { Full, Medium, Low, Danger, Out };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCandleFuelChanged, float, Fraction, ETallowCandleFuelStage, Stage);

// delegates for events
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCandleStateChanged, ECandleState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRelightStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCandleGutteringChanged, bool, bIsGuttering);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAcquireCandle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCandleReset);
/** Matches in the box changed (picked up a box, struck one), or a strike failed with none left. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMatchesChanged, int32, Count, bool, bTriedWithNone);
/** A gust took the flame (Mim came too close): fired after the state has gone Out. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCandleGustedOut);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class TALLOW_API UTallowCandleComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	/** Sets default values for this component's properties */
	UTallowCandleComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	UFUNCTION(BlueprintPure, Category="Candle|Wax") float GetWaxFraction() const { return WaxFraction; }
	UFUNCTION(BlueprintPure, Category="Candle") bool IsRelighting() const { return GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(RelightTimerHandle); }
	UFUNCTION(BlueprintPure, Category="Candle|Wax") ETallowCandleFuelStage GetFuelStage() const;
	UFUNCTION(BlueprintCallable, Category="Candle|Wax") void AddWax(float Fraction);
	/** 31 Oct: off. The candle never burns down; matches are the only thing she can run out of. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Candle|Wax") bool bBurnsDown = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Candle|Wax", meta=(ClampMin="1")) float BurnDurationSeconds = 600.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Candle|Wax", meta=(ClampMin="0.1",ClampMax="1")) float CuppedBurnMultiplier = .6f;
	UPROPERTY(BlueprintAssignable, Category="Candle|Events") FOnCandleFuelChanged OnFuelChanged;

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
		if (!GetWorld()) return false;
		const FTimerManager& TM = GetWorld()->GetTimerManager();
		return TM.IsTimerActive(SprintExtinguishTimerHandle) || TM.IsTimerActive(DraftExtinguishTimerHandle);
	}

	/** Enter and exit drafts */
	UFUNCTION(BlueprintCallable, Category = "Candle")
	void EnterDraft();

	UFUNCTION()
	void ExitDraft();

	/** Get if in draft */
	UFUNCTION(BlueprintPure, Category = "Candle")
	bool IsInDraft() const { return ActiveDraftCount > 0; }

	/** Get candle and check */
	UFUNCTION(BlueprintCallable, Category = "Candle")
	void AcquireCandle(bool bStartLit = true);

	UFUNCTION(BlueprintPure, Category = "Candle")
	bool IsAcquired() const { return bIsAcquired; }

	/** Ending B (TALLOW-LORE.md): Nell snuffs Maisie's candle and leaves it in the collection. It stays out and
	 *  can't be relit; she is still "holding the dark" for everything that asks whether the candle is out. */
	UFUNCTION(BlueprintCallable, Category = "Candle")
	void GiveUp();

	UFUNCTION(BlueprintPure, Category = "Candle")
	bool IsGivenUp() const { return bGivenUp; }

	UFUNCTION(BlueprintCallable, Category = "Candle")
	void ResetCandle();

	/** A capture restarts the house, so the count goes back to none and the boxes go back out. */
	UFUNCTION(BlueprintCallable, Category = "Candle")
	void ResetMatchCount() { MatchCount = 0; OnMatchesChanged.Broadcast(MatchCount, false); }

	/** Matchboxes around the house add to the count (25 Oct). */
	UFUNCTION(BlueprintCallable, Category = "Candle")
	void AddMatches(int32 Count) { MatchCount += FMath::Max(0, Count); OnMatchesChanged.Broadcast(MatchCount, false); }

	UFUNCTION(BlueprintPure, Category = "Candle")
	int32 GetMatchCount() const { return MatchCount; }

	/** Your candle betrays you (29 Oct): a gust as Mim comes right up takes an open flame. Only a lit candle in an
	 *  open hand goes; a cupped one, one mid-strike or one already out is left as it is. Returns true if it went out. */
	UFUNCTION(BlueprintCallable, Category = "Candle")
	bool GustOut();

	UPROPERTY(BlueprintAssignable, Category = "Candle|Events")
	FOnCandleGustedOut OnCandleGustedOut;

	/** A struck match flares brighter than the candle it lights: 1 at the strike, easing to a steady burn, gone soon
	 *  after the wick takes. Mim sees a flare like a lit flame, and further. 0 when no match is burning. */
	UFUNCTION(BlueprintPure, Category = "Candle")
	float GetMatchFlare() const;

	UPROPERTY(BlueprintAssignable, Category = "Candle|Events")
	FOnMatchesChanged OnMatchesChanged;

	/** Exposed delegate properties */
	UPROPERTY(BlueprintAssignable, Category = "Candle|Events")
	FOnCandleStateChanged OnCandleStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Candle|Events")
	FOnRelightStarted OnRelightStarted;

	UPROPERTY(BlueprintAssignable, Category = "Candle|Events")
	FOnCandleGutteringChanged OnCandleGutteringChanged;

	UPROPERTY(BlueprintAssignable, Category = "Candle|Events")
	FOnAcquireCandle OnAcquireCandle;

	UPROPERTY(BlueprintAssignable, Category = "Candle|Events")
	FOnCandleReset OnCandleReset;

    void RestoreForChapter(bool bAcquired, float Wax, int32 Matches, ECandleState State);

protected:
	UPROPERTY(VisibleInstanceOnly, Category="Candle|Wax") float WaxFraction = 1.f;
	/** Current Candle state variable */
	ECandleState CurrentState = ECandleState::Out;

	/** Property for if acquired? */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Candle|State")
	bool bIsAcquired = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Candle|State")
	bool bGivenUp = false;

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

	/** 25 Oct: you start with none; the matchbox by the candle gives 10 and others around the house add more. */
	UPROPERTY(EditDefaultsOnly, Category = "Candle|Tuning")
	int32 MatchCount = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Candle|Tuning")
	bool bUnlimitedMatches = false;

	UPROPERTY(EditDefaultsOnly, Category = "Candle|Tuning")
	float RelightDuration = 1.2f;

	/** How long a struck match's flare lasts (s): from the strike until a little after the wick takes. */
	UPROPERTY(EditDefaultsOnly, Category = "Candle|Tuning")
	float MatchFlareSeconds = 1.6f;

	/** World time the last match was struck. */
	double MatchStruckAt = -100.0;

	FTimerHandle RelightTimerHandle;

	/** Handles candle extinguish on sprint */
	UFUNCTION()
	void HandleSprintExtinguishTimerExpired();

	FTimerHandle SprintExtinguishTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Candle|Tuning")
	float SprintExtinguishGracePeriod = 1.5f;

	/** Count of draft sources, handles candle extinguish on draft */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Candle|State")
	int32 ActiveDraftCount = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Candle|Tuning")
	float DraftExtinguishGracePeriod = 1.5f;

	FTimerHandle DraftExtinguishTimerHandle;

	UFUNCTION()
	void HandleDraftExtinguishTimerExpired();

	/** Called when game starts */
	virtual void BeginPlay() override;

};
