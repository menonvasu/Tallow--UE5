#include "Navigation/TallowNavFilter_PhaseOne.h"
#include "Navigation/TallowNavArea_ChildGap.h"

UTallowNavFilter_PhaseOne::UTallowNavFilter_PhaseOne()
{
	FNavigationFilterArea ChildGap;
	ChildGap.AreaClass = UTallowNavArea_ChildGap::StaticClass();
	ChildGap.bIsExcluded = true;
	Areas.Add(ChildGap);
}
