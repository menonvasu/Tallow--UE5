#include "AI/Hunter/TallowHunterBrain.h"
#include "AI/Hunter/TallowHunterAIController.h"

void UTallowHunterBrain::StartLogic() { bRunning = true; }
void UTallowHunterBrain::RestartLogic()
{
    bRunning = true;
    bPaused = false;
    if (auto* Hunter = Cast<ATallowHunterAIController>(GetOwner())) Hunter->ResetHunt();
}
void UTallowHunterBrain::StopLogic(const FString& Reason)
{
    bRunning = false;
    if (auto* Hunter = Cast<ATallowHunterAIController>(GetOwner())) Hunter->StopMovement();
}
void UTallowHunterBrain::PauseLogic(const FString& Reason)
{
    bPaused = true;
    if (auto* Hunter = Cast<ATallowHunterAIController>(GetOwner())) Hunter->StopMovement();
}
EAILogicResuming::Type UTallowHunterBrain::ResumeLogic(const FString& Reason)
{
    const auto Result = Super::ResumeLogic(Reason);
    bPaused = false;
    bRunning = true;
    return Result;
}
