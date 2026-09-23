#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "TallowAIController.generated.h"

class UAIPerceptionComponent;
class UStateTreeAIComponent;
class UAISenseConfig_Hearing;
class UAISenseConfig_Sight;
class ATallowPatrolPoint;

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

	UFUNCTION(BlueprintCallable, Category = "AI")
	void TriggerPlayerCapture();

	/** Stores Player Actor */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player")
	TObjectPtr<AActor> TargetActor;

	/** Stores the location of the last heard sound*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
	FVector LastHeardNoiseLocation = FVector::ZeroVector;

	/** Stores current patrol point */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI|Patrol")
	TObjectPtr<ATallowPatrolPoint> CurrentPatrolPoint;

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
};
