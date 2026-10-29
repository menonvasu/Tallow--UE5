#include "AI/Hunter/TallowHunterSearchAnchor.h"
#include "Components/SceneComponent.h"

ATallowHunterSearchAnchor::ATallowHunterSearchAnchor()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    PrimaryActorTick.bCanEverTick = false;
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
}
