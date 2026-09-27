#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeTask_TriggerPlayerCapture.generated.h"

class ATallowAIController;

/** Runtime memory for the task: exposed as pins / bindings in the StateTree editor. */
USTRUCT()
struct FStateTreeTask_TriggerPlayerCaptureInstanceData
{
	GENERATED_BODY()

	/** Controller context */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<ATallowAIController> AIController = nullptr;
};

/** Task Logic Struct */
USTRUCT(meta = (DisplayName = "Trigger Player Capture", Category = "Tallow|AI"))
struct TALLOW_API FStateTreeTask_TriggerPlayerCapture : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_TriggerPlayerCaptureInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};