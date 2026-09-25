#pragma once

#include "CoreMinimal.h"
#include "NavAreas/NavArea.h"
#include "TallowNavArea_ChildGap.generated.h"

/**
 * Nav area for child sized routes: vents, gaps under furniture, cupboard backs,
 * holes in the skirting. Placed by ATallowChildGapVolume. The phase one creature
 * filters it out (UTallowNavFilter_PhaseOne), so these routes double as refuges.
 */
UCLASS()
class TALLOW_API UTallowNavArea_ChildGap : public UNavArea
{
	GENERATED_BODY()

public:
	// constructor
	UTallowNavArea_ChildGap();
};
