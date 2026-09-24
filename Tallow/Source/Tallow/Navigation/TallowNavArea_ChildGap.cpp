#include "Navigation/TallowNavArea_ChildGap.h"

UTallowNavArea_ChildGap::UTallowNavArea_ChildGap()
{
	// Normal cost, it is the filter that decides who may use it
	DefaultCost = 1.f;

	// Shows up orange in the navmesh debug view (P in editor)
	DrawColor = FColor(255, 140, 0);
}
