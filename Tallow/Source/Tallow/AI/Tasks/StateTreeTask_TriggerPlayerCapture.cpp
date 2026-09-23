#include "StateTreeTask_TriggerPlayerCapture.h"
#include "StateTreeExecutionContext.h"
#include "AI/TallowAIController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeTask_TriggerPlayerCapture)

EStateTreeRunStatus FStateTreeTask_TriggerPlayerCapture::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

    if (InstanceData.AIController)
    {
        InstanceData.AIController->TriggerPlayerCapture();
    }

    return EStateTreeRunStatus::Succeeded;
}