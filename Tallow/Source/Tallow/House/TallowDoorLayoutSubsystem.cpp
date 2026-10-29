#include "House/TallowDoorLayoutSubsystem.h"
#include "House/TallowHouseRooms.h"
#include "House/TallowBatteryLoopSubsystem.h"
#include "Chapter/TallowChairSpot.h"
#include "Chapter/TallowStoryObject.h"
#include "Environment/TallowPickupSpot.h"
#include "Environment/TallowDoor.h"
#include "Environment/TallowOptionalBoards.h"
#include "Environment/TallowHouseRunLayout.h"
#include "Presentation/TallowRouteGate.h"
#include "Tasks/TallowFuseBox.h"
#include "Engine/World.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "NavigationSystem.h"
#include "Components/SceneComponent.h"

namespace
{
FName SpotName(const AActor* A) {for(FName Tag:A->Tags) if(Tag.ToString().StartsWith(TEXT("Tallow.Spot."))) return FName(*Tag.ToString().RightChop(12));return A->GetFName();}
AActor* LayoutActorNamed(UWorld* W,FName Name) { for(TActorIterator<AActor> I(W);I;++I) if(I->GetFName()==Name||SpotName(*I)==Name) return *I; return nullptr; }
}
bool UTallowDoorLayoutSubsystem::DoesSupportWorldType(const EWorldType::Type Type) const { return Type == EWorldType::Game || Type == EWorldType::PIE; }
UTallowDoorLayoutSubsystem* UTallowDoorLayoutSubsystem::Get(const UObject* Context) { const auto* World = Context ? Context->GetWorld() : nullptr; return World ? World->GetSubsystem<UTallowDoorLayoutSubsystem>() : nullptr; }
void UTallowDoorLayoutSubsystem::GatherSolverInput(FName StartRoom,FTallowSolverInput& Out) const
{
    Out={}; Out.StartRoom=StartRoom;Out.Graph=TallowHouseGraph::Get();
    if(const auto* B=UTallowBatteryLoopSubsystem::Get(this)) {Out.FuseBoxRoom=B->GetFuseBoxRoom();const auto* Box=B->GetFuseBox();Out.bPowerRestored=Box&&Box->HasPower();}
    auto Add=[&](AActor* A,ETallowSpotKind Kind,FName Room=NAME_None,int32 Chapter=-1)
    {
        FTallowSpotCandidate S; S.Spot=SpotName(A);S.Kind=Kind;S.Room=Room.IsNone()?TallowHouseRooms::RoomAt(A->GetActorLocation()):Room;S.Transform=A->GetActorTransform();S.ChapterIndex=Chapter;
        if(!S.Room.IsNone()) Out.Spots.Add(S);
    };
    bool HasBattery=false;
    for(TActorIterator<ATallowPickupSpot> I(GetWorld());I;++I)
    {
        if(!I->IsUsable()) continue;
        const ETallowSpotKind K=I->Item==ETallowPickupSpotItem::Crowbar?ETallowSpotKind::Crowbar:I->Item==ETallowPickupSpotItem::Matchbox?ETallowSpotKind::Matchbox:ETallowSpotKind::Battery;
        Add(*I,K,I->Room);HasBattery|=K==ETallowSpotKind::Battery;
    }
    for(TActorIterator<ATallowChairSpot> I(GetWorld());I;++I) if(I->IsUsable()) Add(*I,ETallowSpotKind::Chair,I->GetRoom());
    for(TActorIterator<ATallowStoryObject> I(GetWorld());I;++I) Add(*I,ETallowSpotKind::Story,I->GetRoom(),I->ChapterIndex);
    for(TActorIterator<ATargetPoint> I(GetWorld());I;++I)
    {
        bool Added=false;
        for(FName Tag:I->Tags)
        {
            const FString T=Tag.ToString();
            if(!HasBattery&&T.StartsWith(TEXT("Tallow.Spawn.Fuse."))) {Add(*I,ETallowSpotKind::Battery);Added=true;}
            else if(T.StartsWith(TEXT("Tallow.Spawn.Crowbar"))) {Add(*I,ETallowSpotKind::Crowbar);Added=true;}
            else if(T.StartsWith(TEXT("Tallow.Spawn.Wax"))) {Add(*I,ETallowSpotKind::Wax);Added=true;}
            else if(T.StartsWith(TEXT("Tallow.MimHuntStart"))||T.StartsWith(TEXT("Tallow.Spawn.Mim"))) {Add(*I,ETallowSpotKind::MimHuntStart);Added=true;}
        }
        if(!Added&&SpotName(*I)==TEXT("Pass2_ChapterStart")) Add(*I,ETallowSpotKind::ChapterStart);
    }
    Out.Spots.Sort([](const auto& A,const auto& B){return A.Spot.LexicalLess(B.Spot);});
    int32 Chairs=0,Stories=0,Batteries=0;for(const auto& S:Out.Spots) {Chairs+=S.Kind==ETallowSpotKind::Chair;Stories+=S.Kind==ETallowSpotKind::Story;Batteries+=S.Kind==ETallowSpotKind::Battery;}
    auto* Nav=UNavigationSystemV1::GetCurrent(GetWorld());
    UE_LOG(LogTemp,Display,TEXT("TALLOW_LAYOUT gathered start=%s spots=%d chairs=%d stories=%d batteries=%d nav_building=%d"),*StartRoom.ToString(),Out.Spots.Num(),Chairs,Stories,Batteries,Nav&&Nav->IsNavigationBuildInProgress());
}
void UTallowDoorLayoutSubsystem::ApplyLayout(const FTallowChapterLayout& L)
{
    AppliedLayout=L;
    for(const auto& E:TallowHouseGraph::Get().Edges)
    {
        const auto State=L.GetDoorState(E.Id,E.DefaultState);
        if(auto* Door=Cast<ATallowDoor>(LayoutActorNamed(GetWorld(),E.DoorActor))) Door->ApplyChapterState(State);
        if(auto* Boards=Cast<ATallowOptionalBoards>(LayoutActorNamed(GetWorld(),E.BoardsActor))) Boards->ApplyChapterBlocked(State==ETallowDoorState::Crowbar);
        if(auto* Gate=Cast<ATallowRouteGate>(LayoutActorNamed(GetWorld(),E.GateActor))) Gate->bRequiresPower=E.bRequiresPower;
    }
    for(TActorIterator<ATallowHouseRunLayout> I(GetWorld());I;++I) I->OnRunDoorsChanged.Broadcast();
    UE_LOG(LogTemp,Display,TEXT("TALLOW_LAYOUT applied seed=%d attempt=%d fallback=%d hash=%d"),L.Seed,L.Attempt,L.bFallback,L.DoorHash());
}
void UTallowDoorLayoutSubsystem::SetChapterControl(bool bControlled)
{
    bChapterControl=bControlled;
    for(TActorIterator<ATallowHouseRunLayout> I(GetWorld());I;++I) I->SetChapterControlled(bControlled);
    for(TActorIterator<ATallowOptionalBoards> I(GetWorld());I;++I) I->SetChapterControlled(bControlled);
}
ETallowDoorState UTallowDoorLayoutSubsystem::GetLiveState(FName Id) const
{
    const auto* E=TallowHouseGraph::Get().Find(Id);if(!E) return ETallowDoorState::StoryGated;
    if(const auto* B=Cast<ATallowOptionalBoards>(LayoutActorNamed(GetWorld(),E->BoardsActor));B&&B->IsRouteBlocked()) return ETallowDoorState::Crowbar;
    if(const auto* D=Cast<ATallowDoor>(LayoutActorNamed(GetWorld(),E->DoorActor)))
    {
        if(D->bChapterSealed) return ETallowDoorState::StoryGated;
        if(D->HasChapterBarricade()) return ETallowDoorState::Crowbar;
        if(D->bIsLocked) return ETallowDoorState::Lockpick;
        return D->IsOpenForChapter()?ETallowDoorState::Open:ETallowDoorState::Closed;
    }
    if(E->bRequiresPower) if(const auto* B=UTallowBatteryLoopSubsystem::Get(this);!B||!B->GetFuseBox()||!B->GetFuseBox()->HasPower()) return ETallowDoorState::StoryGated;
    return E->DefaultState;
}
void UTallowDoorLayoutSubsystem::DescribeLive(TArray<FString>& Lines) const
{
    Lines.Reset();for(const auto& E:TallowHouseGraph::Get().Edges) Lines.Add(FString::Printf(TEXT("%s %s <-> %s state=%d"),*E.Id.ToString(),*E.RoomA.ToString(),*E.RoomB.ToString(),int32(GetLiveState(E.Id))));
}
void UTallowDoorLayoutSubsystem::DrawConnectivity(float Seconds) const
{
#if !UE_BUILD_SHIPPING
    for(const auto& R:TallowHouseRooms::All()) DrawDebugBox(GetWorld(),R.Anchor(),FVector((R.Max.X-R.Min.X)/2,(R.Max.Y-R.Min.Y)/2,20),R.Floor==0?FColor::Blue:R.Floor==1?FColor::Cyan:FColor::Purple,false,Seconds);
    for(const auto& E:TallowHouseGraph::Get().Edges)
    {
        const auto State=GetLiveState(E.Id);const FColor Color=State==ETallowDoorState::Crowbar?FColor::Red:State==ETallowDoorState::Lockpick?FColor::Yellow:State==ETallowDoorState::StoryGated?FColor(80,80,80):FColor::Green;
        for(FName Room:{E.RoomA,E.RoomB}) if(const auto* R=TallowHouseRooms::Find(Room)) DrawDebugLine(GetWorld(),R->Anchor(),E.Doorway,Color,false,Seconds,0,3);
    }
    for(FName Name:{AppliedLayout.ChairSpot,AppliedLayout.BatterySpot}) if(const auto* A=LayoutActorNamed(GetWorld(),Name)) DrawDebugSphere(GetWorld(),A->GetActorLocation(),25,12,FColor::White,false,Seconds);
#endif
}
