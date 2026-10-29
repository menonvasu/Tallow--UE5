#include "House/TallowHouseRooms.h"
FVector FTallowRoomDef::Anchor() const
{
    static const TMap<FName, FVector> Anchors = {
#include "House/TallowHouseRoomAnchors.inl"
    };
    if (const FVector* FloorPoint = Anchors.Find(Id)) return *FloorPoint + FVector(0, 0, 62.f);
    return FVector((Min.X + Max.X) * .5f, (Min.Y + Max.Y) * .5f, TallowHouseRooms::FloorZ(Floor) + 62.f);
}
const TArray<FTallowRoomDef>& TallowHouseRooms::All()
{
    static const TArray<FTallowRoomDef> Rooms = []
    {
        TArray<FTallowRoomDef> R = {
 {TEXT("wake"),TEXT("where i woke"),0,{0,-650},{585,0}},
 {TEXT("workshop"),TEXT("the tools"),0,{0,-1300},{585,-650}},
 {TEXT("cellar_hall"),TEXT("cellar hall"),0,{585,-1300},{975,0}},
 {TEXT("boiler"),TEXT("the humming box"),0,{975,-650},{1575,0}},
 {TEXT("laundry"),TEXT("washing room"),0,{975,-1300},{1575,-650}},
 {TEXT("parlour"),TEXT("the sofa room"),1,{0,-650},{585,0}},
 {TEXT("dining"),TEXT("the long table"),1,{0,-1300},{585,-650}},
 {TEXT("hall"),TEXT("stairs again"),1,{585,-1300},{975,0}},
 {TEXT("study"),TEXT("study"),1,{975,-650},{1575,0}},
 {TEXT("back"),TEXT("kitchen"),1,{975,-1300},{1575,-650}},
 {TEXT("toys"),TEXT("the toys"),2,{0,-650},{585,0}},
 {TEXT("collection"),TEXT("their little places"),2,{0,-1300},{585,-650}},
 {TEXT("upper_hall"),TEXT("top of the stairs"),2,{585,-1300},{975,0}},
 {TEXT("bedroom"),TEXT("the bedroom"),2,{975,-650},{1575,0}},
 {TEXT("upper_back"),TEXT("nursery"),2,{975,-1300},{1575,-650}}};
        for (auto& Room : R)
        {
            if (Room.Id == TEXT("cellar_hall") || Room.Id == TEXT("hall") || Room.Id == TEXT("upper_hall")) Room.bChairAllowed = false;
            if (Room.Id == TEXT("collection")) { Room.bChairAllowed = false; Room.bBatteryAllowed = false; Room.bPowerGated = true; }
        }
        return R;
    }();
    return Rooms;
}
const FTallowRoomDef* TallowHouseRooms::Find(FName Id) { for (const auto& Room : All()) if (Room.Id == Id) return &Room; return nullptr; }
int32 TallowHouseRooms::FloorAt(float Z) { return Z < -100.f ? 0 : Z < 300.f ? 1 : 2; }
float TallowHouseRooms::FloorZ(int32 Floor) { return Floor == 0 ? -330.f : Floor == 1 ? 0.f : 390.f; }
FName TallowHouseRooms::RoomAt(const FVector& Location) { for (const auto& Room : All()) if (Room.Floor == FloorAt(Location.Z) && Room.Contains(Location)) return Room.Id; return NAME_None; }
