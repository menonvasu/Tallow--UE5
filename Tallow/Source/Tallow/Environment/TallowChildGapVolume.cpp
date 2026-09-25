#include "Environment/TallowChildGapVolume.h"
#include "Components/BoxComponent.h"
#include "NavModifierComponent.h"
#include "Navigation/TallowNavArea_ChildGap.h"

ATallowChildGapVolume::ATallowChildGapVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	// Editor shape only, the player and creature must pass straight through it
	GapBox = CreateDefaultSubobject<UBoxComponent>(TEXT("GapBox"));
	GapBox->SetBoxExtent(FVector(50.f, 50.f, 30.f));
	GapBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GapBox->SetCanEverAffectNavigation(false);
	GapBox->ShapeColor = FColor(255, 140, 0);
	RootComponent = GapBox;

	// Uses the owner's bounds, so the modified area follows the box
	NavModifier = CreateDefaultSubobject<UNavModifierComponent>(TEXT("NavModifier"));
	NavModifier->AreaClass = UTallowNavArea_ChildGap::StaticClass();
}
