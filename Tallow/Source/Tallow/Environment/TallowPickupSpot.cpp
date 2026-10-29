#include "Environment/TallowPickupSpot.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"

ATallowPickupSpot::ATallowPickupSpot()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
#if WITH_EDITORONLY_DATA
	Preview = CreateEditorOnlyDefaultSubobject<UStaticMeshComponent>(TEXT("Preview"));
	if (Preview)
	{
		Preview->SetupAttachment(Root);
		Preview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Preview->SetHiddenInGame(true);
		Preview->SetCastShadow(false);
		Preview->bIsEditorOnly = true;
	}
#endif
}

FRotator ATallowPickupSpot::RestingRotation(ETallowPickupSpotItem Kind)
{
	switch (Kind)
	{
	case ETallowPickupSpotItem::Crowbar: return FRotator(0.f, 0.f, 90.f);   // shaft and claw both on the top
	case ETallowPickupSpotItem::Fuse: return FRotator(90.f, 0.f, 0.f);      // the cartridge on its side
	default: return FRotator::ZeroRotator;                                     // matchbox and battery stand as modelled
	}
}

void ATallowPickupSpot::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
#if WITH_EDITORONLY_DATA
	if (!Preview) return;
	Preview->SetStaticMesh(PreviewMesh);
	if (!PreviewMesh) return;
	// The same rest the director gives the real item: lying as it lies, middle over the spot, bottom on the top.
	const FRotator Rest = RestingRotation(Item);
	const FBox Bounds = PreviewMesh->GetBoundingBox().TransformBy(FTransform(Rest));
	Preview->SetRelativeLocationAndRotation(FVector(-Bounds.GetCenter().X, -Bounds.GetCenter().Y, -Bounds.Min.Z), Rest);
#endif
}

bool ATallowPickupSpot::IsUsable() const
{
	if (!IsValid(Surface) || Surface->IsHidden()) return false;
	const USceneComponent* Top = Surface->GetRootComponent();
	return !Top || Top->IsVisible();
}

void ATallowPickupSpot::SeatItem(AActor* Pickup) const
{
	if (!Pickup) return;
	const FQuat Turn = FRotator(0.f, GetActorRotation().Yaw, 0.f).Quaternion() * RestingRotation(Item).Quaternion();
	Pickup->SetActorLocationAndRotation(GetActorLocation(), Turn);
	FBox Bounds(ForceInit);
	for (const UStaticMeshComponent* Mesh : TInlineComponentArray<UStaticMeshComponent*>(Pickup))
		if (Mesh->GetStaticMesh() && Mesh->IsVisible() && !Mesh->IsEditorOnly())
			Bounds += Mesh->CalcBounds(Mesh->GetComponentTransform()).GetBox();
	if (!Bounds.IsValid) return;
	const FVector Spot = GetActorLocation();
	Pickup->AddActorWorldOffset(FVector(Spot.X - Bounds.GetCenter().X, Spot.Y - Bounds.GetCenter().Y, Spot.Z - Bounds.Min.Z));
}

bool ATallowPickupSpot::IsReachableFrom(const FVector& From) const
{
	const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), From, GetActorTransform().TransformPosition(Approach));
	return Path && Path->IsValid() && !Path->IsPartial();
}

ATallowPickupSpot* ATallowPickupSpot::PickSpot(const UObject* WorldContext, ETallowPickupSpotItem Kind, FName OnFloor, const TArray<FName>& ExcludeRooms,
	const TArray<ATallowPickupSpot*>& Taken, FRandomStream& Random, bool bMustReach, const FVector& ReachFrom)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return nullptr;
	TArray<ATallowPickupSpot*> Spots;
	for (TActorIterator<ATallowPickupSpot> It(World); It; ++It)
	{
		if (It->Item != Kind || (!OnFloor.IsNone() && It->Floor != OnFloor) || ExcludeRooms.Contains(It->Room) || Taken.Contains(*It)) continue;
		if (!It->IsUsable()) { UE_LOG(LogTemp, Warning, TEXT("TALLOW_INTEGRATION pickup spot %s has lost its surface"), *It->GetName()); continue; }
		Spots.Add(*It);
	}
	Spots.Sort([](const ATallowPickupSpot& A, const ATallowPickupSpot& B) { return A.GetName() < B.GetName(); });
	if (bMustReach) Spots.RemoveAll([&](const ATallowPickupSpot* Spot) { return !Spot->IsReachableFrom(ReachFrom); });
	return Spots.Num() ? Spots[Random.RandRange(0, Spots.Num()-1)] : nullptr;
}

ATallowPickupSpot* ATallowPickupSpot::PickSeededSpot(const UObject* WorldContext, ETallowPickupSpotItem Kind, FName OnFloor, const TArray<FName>& ExcludeRooms,
	const TArray<ATallowPickupSpot*>& Taken, int32 Seed, bool bMustReach, FVector ReachFrom)
{
	FRandomStream Random(Seed);
	return PickSpot(WorldContext, Kind, OnFloor, ExcludeRooms, Taken, Random, bMustReach, ReachFrom);
}
