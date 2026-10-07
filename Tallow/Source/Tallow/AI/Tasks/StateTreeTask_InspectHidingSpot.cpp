#include "StateTreeTask_InspectHidingSpot.h"
#include "StateTreeExecutionContext.h"
#include "AI/TallowAIController.h"
#include "AI/TallowCreature.h"
#include "Environment/TallowHideVolume.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeTask_InspectHidingSpot)

EStateTreeRunStatus FStateTreeTask_InspectHidingSpot::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.AIController == nullptr) { return EStateTreeRunStatus::Failed; }
	
	ATallowCreature* Creature = Cast<ATallowCreature>(InstanceData.AIController->GetPawn());
	if (!Creature)  { return EStateTreeRunStatus::Failed; }
	
	switch (InstanceData.HidingSpotType)
	{
		case ETallowHidingSpotType::UnderBed:
			Creature->SetCreatureVisualState(ETallowCreatureState::SearchLow);
			break;
		case ETallowHidingSpotType::Wardrobe:
			Creature->SetCreatureVisualState(ETallowCreatureState::SearchHigh);
			break;
		default:
			Creature->SetCreatureVisualState(ETallowCreatureState::Investigating);
			break;
	}
	
	return EStateTreeRunStatus::Succeeded;
}

void FStateTreeTask_InspectHidingSpot::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (InstanceData.AIController == nullptr) { return; }
	
	ATallowCreature* Creature = Cast<ATallowCreature>(InstanceData.AIController->GetPawn());
	if (!Creature) { return; }
	
	Creature->SetCreatureVisualState(ETallowCreatureState::Investigating);
}