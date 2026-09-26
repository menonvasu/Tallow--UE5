#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "StateTreeExecutionContext.h"
#include "AI/TallowCreature.h"
#include "StateTreeTask_SetCreatureState.generated.h"

class ATallowAIController;

/** Runtime memory for the task: exposed as pins / bindings in the StateTree editor. */
USTRUCT()
struct FStateTreeTask_SetCreatureStateInstanceData
{
	GENERATED_BODY()

	/** Controller context */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<ATallowAIController> AIController = nullptr;

	/** Input state */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	ETallowCreatureState NewState = ETallowCreatureState::Patrol;
};

/** Task Logic Struct */
USTRUCT(meta = (DisplayName = "Set Creature State", Category = "Tallow|AI"))
struct TALLOW_API FStateTreeTask_SetCreatureState : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_SetCreatureStateInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};