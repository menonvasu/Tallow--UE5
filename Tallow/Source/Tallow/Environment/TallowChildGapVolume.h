#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TallowChildGapVolume.generated.h"

class UBoxComponent;
class UNavModifierComponent;

/**
 * Drop over a vent, a gap under furniture or a hole in the skirting and size the
 * box to fit. The navmesh inside it becomes a child gap area, which the phase one
 * creature cannot path through (issue #23).
 */
UCLASS()
class TALLOW_API ATallowChildGapVolume : public AActor
{
	GENERATED_BODY()

public:
	// constructor
	ATallowChildGapVolume();

protected:

	/** The gap. Size this to the opening, it has no collision */
	UPROPERTY(VisibleAnywhere, Category = "ChildGap|Components")
	TObjectPtr<UBoxComponent> GapBox;

	/** Stamps the child gap nav area onto the navmesh inside the box */
	UPROPERTY(VisibleAnywhere, Category = "ChildGap|Components")
	TObjectPtr<UNavModifierComponent> NavModifier;
};
