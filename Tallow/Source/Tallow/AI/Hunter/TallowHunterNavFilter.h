#pragma once
#include "CoreMinimal.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "TallowHunterNavFilter.generated.h"

/** Mim can share feasible child passages, with their much slower crawl cost. */
UCLASS()
class TALLOW_API UTallowHunterNavFilter : public UNavigationQueryFilter
{
    GENERATED_BODY()
public:
    UTallowHunterNavFilter();
};
