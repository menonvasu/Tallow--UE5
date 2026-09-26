#include "StateTreeTask_SetCreatureState.h"
#include "StateTreeExecutionContext.h"
#include "AI/TallowCreature.h"
#include "AI/TallowAIController.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeTask_SetCreatureState)

EStateTreeRunStatus FStateTreeTask_SetCreatureState::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (InstanceData.AIController == nullptr) return EStateTreeRunStatus::Failed;

	ATallowCreature* Creature = Cast<ATallowCreature>(InstanceData.AIController->GetPawn());
	if (!Creature) return EStateTreeRunStatus::Failed; 

	Creature->SetCreatureVisualState(InstanceData.NewState);
	return EStateTreeRunStatus::Succeeded;
}
