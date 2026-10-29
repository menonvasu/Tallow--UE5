#include "Navigation/TallowDoorNavModifier.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "NavAreas/NavArea_Null.h"

UTallowDoorNavModifier::UTallowDoorNavModifier()
{
    AreaClass = UNavArea_Null::StaticClass();
    bIncludeAgentHeight = true;
    bAttachToOwnersRoot = false;
    SetNavigationRelevancy(false);
}

void UTallowDoorNavModifier::RefreshLeafBounds()
{
    bBoundsInitialized = false;
    RefreshNavigationModifiers();
}

void UTallowDoorNavModifier::CalculateBounds() const
{
    Bounds = FBox(ForceInit);
    ComponentBounds.Reset();
    if (const AActor* Owner = GetOwner())
        for (const UPrimitiveComponent* Part : TInlineComponentArray<UPrimitiveComponent*>(Owner))
            if (Part->GetFName() == TEXT("DoorMesh"))
            {
                // The frame and AI proximity volume are much larger than the
                // door. Only the leaf's strip should stop a navigation route.
                // The modifier also supplies a small margin at the open leaf's coarse navmesh corners.
                Bounds = Part->Bounds.GetBox().ExpandBy(FVector(12.f, 12.f, 4.f));
                ComponentBounds.Add(FRotatedBox(Bounds, FQuat::Identity));
                break;
            }
}
