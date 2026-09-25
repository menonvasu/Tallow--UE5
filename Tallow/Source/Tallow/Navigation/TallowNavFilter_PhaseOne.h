#pragma once

#include "CoreMinimal.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "TallowNavFilter_PhaseOne.generated.h"

/**
 * Pathfinding filter for the phase one creature. Excludes child gap areas so it
 * can never path through a vent or under the furniture. Phase two clears the
 * filter (ATallowCreature::SetPhase) and the spider form can follow you in.
 */
UCLASS()
class TALLOW_API UTallowNavFilter_PhaseOne : public UNavigationQueryFilter
{
	GENERATED_BODY()

public:
	// constructor
	UTallowNavFilter_PhaseOne();
};
