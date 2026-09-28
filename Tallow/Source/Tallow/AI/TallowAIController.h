#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TallowCreature.h"
#include "AIController.h"
#include "TallowAIController.generated.h"

class UAIPerceptionComponent;
class UStateTreeAIComponent;
class UAISenseConfig_Hearing;
class UAISenseConfig_Sight;
class ATallowPatrolPoint;
class ATallowCharacter;

/**
 * 
 */
UCLASS()
class TALLOW_API ATallowAIController : public AAIController
{
	GENERATED_BODY()

public:

	/** Constructor */
	ATallowAIController();

	virtual void BeginPlay() override;

	virtual void OnPossess(APawn* InPawn) override;

	void ResetBrain();

	/** Called when perception detected */
	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* SourceActor, FAIStimulus Stimulus);

	/** Phase one keeps out of child gaps, phase two may use them (#23) */
	void ApplyPhaseNavigationFilter(ETallowCreaturePhase Phase);
	
	UFUNCTION(BlueprintCallable, Category = "AI")
	void TriggerPlayerCapture();

	void NotifyPlayerTouched(AActor* TouchedActor);

	/** Event tag the StateTree uses for 'player spotted' (also sent by the Keeper's close-range sense) */
	FGameplayTag GetPlayerSpottedTag() const { return PlayerSpottedTag; }

	/** Stores Player Actor */ 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	TObjectPtr<AActor> TargetActor;

	/** Stores the location of the last heard sound*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
	FVector LastHeardNoiseLocation = FVector::ZeroVector;

	/** Stores last search location for better search */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI|Search")
	FVector LastSearchLocation = FVector::ZeroVector;

	/** Stores current patrol point */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI|Patrol")
	TObjectPtr<ATallowPatrolPoint> CurrentPatrolPoint;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI|Search")
	int32 CurrentSearchCount;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Search")
	int32 MaxSearchSpots = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	bool bIsTargetTiedToChair = false;

protected:

	/** Perception Component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAIPerceptionComponent> AIPerceptionComponent;

	/** State Tree Component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStateTreeAIComponent> StateTreeAIComponent;

	/** Hearing Configuration Component*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI Perception")
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	/** Sight Configuration Component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI Perception")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	/** Tag to set in StateTree for noise heard */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Events")
	FGameplayTag NoiseHeardTag;

	/** Player sight spotted tags */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Events")
	FGameplayTag PlayerSpottedTag;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Events")
	FGameplayTag PlayerLostTag;

	/** Pathfinding filter while in phase one. Excludes child gaps by default */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Navigation")
	TSubclassOf<UNavigationQueryFilter> PhaseOneNavigationFilter;

	/** Pathfinding filter while in phase two. Empty means every route is open */
	UPROPERTY(EditDefaultsOnly, Category = "AI|Navigation")
	TSubclassOf<UNavigationQueryFilter> PhaseTwoNavigationFilter;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Events")
	FGameplayTag PlayerCaughtTag;

	/** Variables to reduce path retrack */
	double LastNoiseProcessTime = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Tuning")
	float MinNoiseRepathDelay = 0.6f; // Min seconds between path updates

	UPROPERTY(EditDefaultsOnly, Category = "AI|Tuning")
	float NoiseRepathDistanceThreshold = 200.0f; // Min distance to care about new steps
};
