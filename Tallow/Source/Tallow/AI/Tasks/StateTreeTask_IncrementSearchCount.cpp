#include "StateTreeTask_IncrementSearchCount.h"
#include "StateTreeExecutionContext.h"
#include "AI/TallowAIController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeTask_IncrementSearchCount)

EStateTreeRunStatus FStateTreeTask_IncrementSearchCount::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (InstanceData.AIController == nullptr) return EStateTreeRunStatus::Failed;

	InstanceData.AIController->LastSearchLocation = InstanceData.AIController->GetPawn()->GetActorLocation();
	InstanceData.AIController->CurrentSearchCount++;
	return EStateTreeRunStatus::Succeeded;
}
