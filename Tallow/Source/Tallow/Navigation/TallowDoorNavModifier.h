#pragma once
#include "CoreMinimal.h"
#include "NavModifierComponent.h"
#include "TallowDoorNavModifier.generated.h"

/** A shut lock, barricade or resting open leaf removes its occupied strip from route planning. */
UCLASS()
class TALLOW_API UTallowDoorNavModifier : public UNavModifierComponent
{
    GENERATED_BODY()
public:
    UTallowDoorNavModifier();
    virtual void CalculateBounds() const override;
    void RefreshLeafBounds();
};
