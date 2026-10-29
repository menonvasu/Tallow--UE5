#include "AI/Hunter/TallowHunterNavFilter.h"
#include "Navigation/TallowNavArea_ChildGap.h"

UTallowHunterNavFilter::UTallowHunterNavFilter()
{
    FNavigationFilterArea Gap;
    Gap.AreaClass = UTallowNavArea_ChildGap::StaticClass();
    Gap.bIsExcluded = false;
    Gap.bOverrideTravelCost = true;
    Gap.TravelCostOverride = 6.25f;
    Gap.bOverrideEnteringCost = true;
    Gap.EnteringCostOverride = 150.f;
    Areas.Add(Gap);
}
