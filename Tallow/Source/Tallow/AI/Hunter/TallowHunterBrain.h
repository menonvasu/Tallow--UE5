#pragma once
#include "CoreMinimal.h"
#include "BrainComponent.h"
#include "TallowHunterBrain.generated.h"

UCLASS()
class TALLOW_API UTallowHunterBrain : public UBrainComponent
{
    GENERATED_BODY()
public:
    virtual void StartLogic() override;
    virtual void RestartLogic() override;
    virtual void StopLogic(const FString& Reason) override;
    virtual void PauseLogic(const FString& Reason) override;
    virtual EAILogicResuming::Type ResumeLogic(const FString& Reason) override;
    virtual bool IsRunning() const override { return bRunning && !bPaused; }
    virtual bool IsPaused() const override { return bPaused; }
private:
    bool bRunning = false;
    bool bPaused = false;
};
