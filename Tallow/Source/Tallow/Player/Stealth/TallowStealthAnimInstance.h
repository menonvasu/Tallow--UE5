#pragma once

#include "CoreMinimal.h"
#include "Animation/Presentation/TallowPresentationAnimInstance.h"
#include "TallowStealthAnimInstance.generated.h"

/** Retains the presentation director's clip API and layers a real ledge/wall hand contact. */
UCLASS(Transient)
class TALLOW_API UTallowStealthAnimInstance : public UTallowPresentationAnimInstance
{
    GENERATED_BODY()
public:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
