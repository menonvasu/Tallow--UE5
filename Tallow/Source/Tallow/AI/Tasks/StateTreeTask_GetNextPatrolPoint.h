#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeTask_GetNextPatrolPoint.generated.h"

class ATallowAIController;

/** Runtime memory for the task: exposed as pins / bindings in the StateTree editor. */
USTRUCT()
struct FStateTreeTask_GetNextPatrolPointInstanceData
{
	GENERATED_BODY()

	/** Controller context */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<ATallowAIController> AIController = nullptr;

	/** Output location */
	UPROPERTY(EditAnywhere, Category = "Output")
	FVector NextPatrolLocation = FVector::ZeroVector;
};

/** Task Logic Struct */
USTRUCT(meta = (DisplayName = "Get Next Patrol Point", Category = "Tallow|AI"))
struct TALLOW_API FStateTreeTask_GetNextPatrolPoint : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_GetNextPatrolPointInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};