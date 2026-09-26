#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeTask_IncrementSearchCount.generated.h"

class ATallowAIController;

/** Runtime memory for the task: exposed as pins / bindings in the StateTree editor. */
USTRUCT()
struct FStateTreeTask_IncrementSearchCountInstanceData
{
	GENERATED_BODY()

	/** Controller context */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<ATallowAIController> AIController = nullptr;

};

/** Task Logic Struct */
USTRUCT(meta = (DisplayName = "Increment search count", Category = "Tallow|AI"))
struct TALLOW_API FStateTreeTask_IncrementSearchCount : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_IncrementSearchCountInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
