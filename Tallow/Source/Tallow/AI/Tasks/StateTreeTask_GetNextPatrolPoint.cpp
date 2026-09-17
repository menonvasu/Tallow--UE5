#include "StateTreeTask_GetNextPatrolPoint.h"
#include "StateTreeExecutionContext.h"
#include "PatrolPoint/TallowPatrolPoint.h"
#include "AI/TallowAIController.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeTask_GetNextPatrolPoint)

EStateTreeRunStatus FStateTreeTask_GetNextPatrolPoint::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (InstanceData.AIController == nullptr || InstanceData.AIController->CurrentPatrolPoint == nullptr ) return EStateTreeRunStatus::Failed;

	ATallowPatrolPoint* NextPoint = InstanceData.AIController->CurrentPatrolPoint->GetNextPoint();
	if (NextPoint == nullptr) return EStateTreeRunStatus::Failed;
	
	InstanceData.AIController->CurrentPatrolPoint = NextPoint;
	InstanceData.NextPatrolLocation = NextPoint->GetActorLocation();
	return EStateTreeRunStatus::Succeeded;
}	
