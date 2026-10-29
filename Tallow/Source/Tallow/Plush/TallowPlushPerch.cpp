#include "Plush/TallowPlushPerch.h"
#include "House/TallowHouseRooms.h"
#include "Components/SceneComponent.h"
ATallowPlushPerch::ATallowPlushPerch() { SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root"))); }
FName ATallowPlushPerch::GetRoom() const { return RoomOverride.IsNone() ? TallowHouseRooms::RoomAt(GetActorLocation()) : RoomOverride; }
