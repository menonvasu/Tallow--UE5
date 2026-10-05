#pragma once

#include "CoreMinimal.h"
#include "AI/Evaluators/StateTreeEvaluator_HidingSpotDetails.h"
#include "StateTreeTaskBase.h"
#include "Environment/TallowHideVolume.h"
#include "StateTreeTask_InspectHidingSpot.generated.h"

class ATallowCreature;
class ATallowAIController;

USTRUCT()
struct FStateTreeTask_InspectHidingSpotInstanceData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = Input)
	ETallowHidingSpotType HidingSpotType = ETallowHidingSpotType::Wardrobe;
	
	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<ATallowAIController> AIController = nullptr;
};

USTRUCT(meta = (DisplayName = "Inspect Hiding Spot", Category = "Tallow|AI"))
struct TALLOW_API FStateTreeTask_InspectHidingSpot : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()
	
	using FInstanceDataType = FStateTreeTask_InspectHidingSpotInstanceData;
	
	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}
	
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
