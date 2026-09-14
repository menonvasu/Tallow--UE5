#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "TallowAIController.generated.h"

class UAIPerceptionComponent;
class UStateTreeComponent;
class UAISenseConfig_Hearing;

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

	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* SourceActor, FAIStimulus Stimulus);

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAIPerceptionComponent> AIPerceptionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStateTreeComponent> StateTreeComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI Perception")
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI")
	FVector LastHeardNoiseLocation = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Events")
	FGameplayTag NoiseHeardTag;
};
