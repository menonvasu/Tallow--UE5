#include "StateTreeEvaluator_HidingSpotDetails.h"
#include "StateTreeExecutionContext.h"
#include "Environment/TallowHideVolume.h"

void FStateTreeEvaluator_HidingSpotDetails::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FStateTreeEvaluator_HidingSpotDetailsInstanceData& InstanceData = Context.GetInstanceData<FStateTreeEvaluator_HidingSpotDetailsInstanceData>(*this);

	// Exit early if actor hasn't changed for performace
	if (InstanceData.HideActor == InstanceData.LastEvaluatedActor)
	{
		return;
	}

	InstanceData.LastEvaluatedActor = InstanceData.HideActor;

	ATallowHideVolume* HideVolume = Cast<ATallowHideVolume>(InstanceData.HideActor);
	if (!HideVolume) return;

	if (USceneComponent* ExitPoint = HideVolume->GetExitPoint())
	{	
		InstanceData.ApproachLocation = ExitPoint->GetComponentLocation();
		InstanceData.SpotType = HideVolume->GetHidingSpotType();
	}
}
