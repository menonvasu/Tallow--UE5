#pragma once
#include "CoreMinimal.h"
#include "Animation/Presentation/TallowPresentationAnimInstance.h"
#include "TallowHunterAnimInstance.generated.h"

/** Sequence playback plus a small, thread-safe contact/clearance layer. */
UCLASS(Transient)
class TALLOW_API UTallowHunterAnimInstance : public UTallowPresentationAnimInstance
{
    GENERATED_BODY()
public:
    bool PlayMotionClip(UAnimSequence* Clip, bool bLoop, float Rate, float BlendSeconds, bool bMatchPhase);
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
private:
    friend struct FTallowHunterAnimProxy;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> MotionClip;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> OutgoingClip;
    float OutgoingTime = 0.f, OutgoingRate = 1.f, MotionRate = 1.f;
    float TransitionAge = 1.f, TransitionDuration = .4f;
    bool bMotionLoop = true, bOutgoingLoop = true, bInterrupted = false;
    uint32 MotionGeneration = 0;
};
