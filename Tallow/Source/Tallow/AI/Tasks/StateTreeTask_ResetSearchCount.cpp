#include "StateTreeTask_ResetSearchCount.h"
#include "StateTreeExecutionContext.h"
#include "AI/TallowAIController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeTask_ResetSearchCount)

EStateTreeRunStatus FStateTreeTask_ResetSearchCount::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (InstanceData.AIController == nullptr) return EStateTreeRunStatus::Failed;

	InstanceData.AIController->CurrentSearchCount = 0;
	InstanceData.AIController->LastSearchLocation = FVector(999999.f);
	return EStateTreeRunStatus::Succeeded;
}
