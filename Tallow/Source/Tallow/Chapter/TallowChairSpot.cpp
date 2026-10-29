#include "Chapter/TallowChairSpot.h"
#include "House/TallowHouseRooms.h"
#include "House/TallowHouseGraph.h"
#include "Tasks/TallowChairEscape.h"
#include "AI/TallowCreature.h"
#include "Player/TallowCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
ATallowChairSpot::ATallowChairSpot()
{
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root")); SetRootComponent(Root);
    CandlePlace = CreateDefaultSubobject<USceneComponent>(TEXT("CandlePlace")); CandlePlace->SetupAttachment(Root);
    CandlePlace->SetRelativeLocation(FVector(40,65,0));
}
FName ATallowChairSpot::GetRoom() const { return RoomOverride.IsNone() ? TallowHouseRooms::RoomAt(GetActorLocation()) : RoomOverride; }
bool ATallowChairSpot::IsUsable() const
{
    auto* Chair=Cast<ATallowChairEscape>(UGameplayStatics::GetActorOfClass(this,ATallowChairEscape::StaticClass()));
    float Clearance=0; FString Problem;
    return bEnabled && ValidatePlacement(GetWorld(),Chair,GetActorTransform(),false,Clearance,Problem);
}
bool ATallowChairSpot::ValidatePlacement(UWorld* World, ATallowChairEscape* Chair, const FTransform& Transform,
    bool bFullCheck, float& OutClearance, FString& OutProblem, FVector* OutMim)
{
    OutClearance=0.f; OutProblem.Reset();
    auto Fail=[&OutProblem](const FString& Why) { OutProblem=Why; return false; };
    auto* Nav=UNavigationSystemV1::GetCurrent(World);
    if (!World || !Chair || !Nav || !Chair->GetChairMesh() || !Chair->GetChairMesh()->GetStaticMesh()) return Fail(TEXT("missing chair mesh or navigation"));
    auto* Player=Cast<ATallowCharacter>(UGameplayStatics::GetPlayerCharacter(World,0));
    auto* Mim=Cast<ATallowCreature>(UGameplayStatics::GetActorOfClass(World,ATallowCreature::StaticClass()));
    FCollisionQueryParams Params(TEXT("ChapterChair"),false,Chair); Params.AddIgnoredActor(Player); Params.AddIgnoredActor(Mim);
    FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_WorldStatic); Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    const FTransform MeshTransform=Chair->GetStandingMeshRelativeTransform()*Transform;
    const FBox Bounds=Chair->GetChairMesh()->GetStaticMesh()->GetBoundingBox();
    const FVector Extent=Bounds.GetExtent()*MeshTransform.GetScale3D().GetAbs()+FVector(4.f);
    const FVector Center=MeshTransform.TransformPosition(Bounds.GetCenter());
    TArray<FOverlapResult> Overlaps;
    World->OverlapMultiByObjectType(Overlaps,Center,MeshTransform.GetRotation(),Objects,FCollisionShape::MakeBox(Extent),Params);
    // The floor supports the chair. Four centimetres of clearance applies to surrounding geometry, not the support plane.
    FHitResult Support;
    World->LineTraceSingleByChannel(Support,Transform.GetLocation()+FVector(0,0,20),Transform.GetLocation()-FVector(0,0,30),ECC_Visibility,Params);
    for (const auto& Hit : Overlaps)
    {
        const auto* Component=Hit.GetComponent();
        if (!Component || Component->GetCollisionResponseToChannel(ECC_Pawn)!=ECR_Block) continue;
        if (Component==Support.GetComponent() && Support.ImpactNormal.Z>.9f
            && Component->Bounds.Origin.Z+Component->Bounds.BoxExtent.Z<=Support.ImpactPoint.Z+1.f) continue;
        return Fail(FString::Printf(TEXT("chair overlap %s"),*GetNameSafe(Hit.GetActor())));
    }
    const FTransform Exit=Chair->GetExitRelativeTransform()*Transform;
    const float Radius=Player ? Player->GetCapsuleComponent()->GetScaledCapsuleRadius() : 30.f;
    const float Half=Player ? Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 60.f;
    if (World->OverlapBlockingTestByChannel(Exit.GetLocation(),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half-.5f),Params)) return Fail(TEXT("exit capsule blocked"));
    FNavLocation ExitNav;
    if (!Nav->ProjectPointToNavigation(Exit.GetLocation(),ExitNav,FVector(25,25,100))) return Fail(TEXT("exit off navmesh"));
    const auto* Room=TallowHouseRooms::Find(TallowHouseRooms::RoomAt(Transform.GetLocation()));
    if (!Room) return Fail(TEXT("outside room"));
    FNavLocation Anchor;
    if (!Nav->ProjectPointToNavigation(Room->Anchor()-FVector(0,0,62),Anchor,FVector(30,30,60))) return Fail(TEXT("room anchor off navmesh"));
    auto* Path=UNavigationSystemV1::FindPathToLocationSynchronously(World,ExitNav.Location,Anchor.Location);
    if (!Path || !Path->IsValid() || Path->IsPartial()) return Fail(TEXT("exit has no room path"));
    // A seated capsule must also clear the room. Chair contact is intentional and excluded above.
    const FTransform Seat=Chair->GetSeatRelativeTransform()*Transform;
    TArray<FOverlapResult> SeatOverlaps;
    World->OverlapMultiByChannel(SeatOverlaps,Seat.GetLocation(),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half-.5f),Params);
    for (const auto& Hit:SeatOverlaps)
        if (Hit.bBlockingHit && !(Hit.GetComponent()==Support.GetComponent() && Support.ImpactNormal.Z>.9f
            && Hit.GetComponent()->Bounds.Origin.Z+Hit.GetComponent()->Bounds.BoxExtent.Z<=Support.ImpactPoint.Z+1.f))
            return Fail(FString::Printf(TEXT("seat capsule blocked %s"),*GetNameSafe(Hit.GetActor())));
    if (!bFullCheck) return true;
    const FVector Eye=Seat.GetLocation()+FVector(0,0,48);
    for (int32 Yaw=-70;Yaw<=70;Yaw+=10) for (int32 Pitch=-38;Pitch<=0;Pitch+=19)
    {
        const FVector Direction=FRotator(Pitch,Transform.Rotator().Yaw+Yaw,0).Vector(); FHitResult Hit;
        if (World->SweepSingleByChannel(Hit,Eye,Eye+Direction*160.f,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(12),Params))
        { if (Hit.Distance<60.f) return Fail(TEXT("seated eye obstructed")); OutClearance+=Hit.Distance; }
        else OutClearance+=160.f;
    }
    auto DoorClear=[&](const FVector& Door) { return TallowHouseRooms::FloorAt(Door.Z)!=Room->Floor || FVector::Dist2D(Transform.GetLocation(),Door)>=120.f; };
    for (const auto& Edge:TallowHouseGraph::Get().Edges) if (!DoorClear(Edge.Doorway)) return Fail(TEXT("near graph doorway"));
    static const FVector Openings[]={
#include "Chapter/TallowChairDoorways.inl"
    };
    for (const auto& Door:Openings) if (!DoorClear(Door)) return Fail(TEXT("near measured opening"));
    const auto* DefaultMim=Mim ? Mim->GetClass()->GetDefaultObject<ATallowCreature>() : nullptr;
    const float MimScale=Mim ? Mim->GetCapsuleComponent()->GetShapeScale() : 1.f;
    const float MimRadius=Mim ? FMath::Max(Mim->GetCapsuleComponent()->GetUnscaledCapsuleRadius(),DefaultMim->GetCapsuleComponent()->GetUnscaledCapsuleRadius())*MimScale : 45.f;
    const float MimHalf=Mim ? FMath::Max(Mim->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(),DefaultMim->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight())*MimScale : 100.f;
    for (float Distance=150.f;Distance<=400.f;Distance+=25.f)
    {
        FNavLocation Spot; const FVector Goal=Transform.GetLocation()+Transform.GetRotation().GetForwardVector()*Distance;
        if (!Nav->ProjectPointToNavigation(Goal,Spot,FVector(30,30,100))) continue;
        if (FMath::Abs(Spot.Location.Z-TallowHouseRooms::FloorZ(Room->Floor))>35.f || TallowHouseRooms::RoomAt(Spot.Location)!=Room->Id) continue;
        const FVector Standing=Spot.Location+FVector(0,0,MimHalf);
        if (World->OverlapBlockingTestByChannel(Standing,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(MimRadius,MimHalf-.5f),Params)) continue;
        OutClearance+=Distance; if (OutMim) *OutMim=Standing; return true;
    }
    return Fail(TEXT("no Mim capsule in front"));
}
