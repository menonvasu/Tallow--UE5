#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TallowPickupSpot.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ETallowPickupSpotItem : uint8 { Crowbar, Fuse, Matchbox, Battery };

/** One authored place a pickup can start a run (28 Oct: "on specific surfaces we've made, where we know
 * exactly where the part they can sit on is"). It stands on Surface, at the height of its top, with the item's
 * middle over it, turned the way the item lies. The director picks among these each run; a spot whose surface
 * has gone or is hidden is never used. Attached to its surface, so moving the furniture carries it along. */
UCLASS()
class TALLOW_API ATallowPickupSpot : public AActor
{
	GENERATED_BODY()

public:
	ATallowPickupSpot();
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spot")
	ETallowPickupSpotItem Item = ETallowPickupSpotItem::Fuse;

	/** Basement, Ground or Upper. Each fuse takes one floor; a matchbox stays on its own floor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spot")
	FName Floor;

	/** Stable house room ID (for example "boiler"), so a chapter can exclude rooms. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spot")
	FName Room;

	/** The bench, shelf or table top the item rests on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spot")
	TObjectPtr<AActor> Surface;

	/** Where it is, in words, for the run log ("workshop bench, left end"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spot")
	FString Place;

	/** Where Nell stands to take it (on the floor, relative to the spot): the measured place her reach meets it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spot", meta = (MakeEditWidget))
	FVector Approach = FVector::ZeroVector;

	/** Shown resting on the spot in the editor only. */
	UPROPERTY(EditAnywhere, Category = "Spot")
	TObjectPtr<UStaticMesh> PreviewMesh;

	/** The surface is still in the house and seen. */
	UFUNCTION(BlueprintPure, Category = "Spot")
	bool IsUsable() const;

	/** A whole navmesh walk from From to where Nell stands to take it. Locked and barricaded doors are barriers
	 * on the navmesh, so nothing behind one counts. */
	UFUNCTION(BlueprintPure, Category = "Spot")
	bool IsReachableFrom(const FVector& From) const;

	/** Moves Pickup onto the spot: turned to its yaw, its mesh centred over it, its lowest point on the top. */
	UFUNCTION(BlueprintCallable, Category = "Spot")
	void SeatItem(AActor* Pickup) const;

	/** A random usable spot for Kind, on OnFloor (any floor when None), in none of ExcludeRooms, not one of Taken,
	 * and, with bMustReach, reachable from ReachFrom. Candidates are sorted by name first, so a seed always picks
	 * the same. Null when nothing qualifies. */
	static ATallowPickupSpot* PickSpot(const UObject* WorldContext, ETallowPickupSpotItem Kind, FName OnFloor, const TArray<FName>& ExcludeRooms,
		const TArray<ATallowPickupSpot*>& Taken, FRandomStream& Random, bool bMustReach = false, const FVector& ReachFrom = FVector::ZeroVector);

	UFUNCTION(BlueprintCallable, Category = "Spot", meta = (WorldContext = "WorldContext", AutoCreateRefTerm = "ExcludeRooms,Taken"))
	static ATallowPickupSpot* PickSeededSpot(const UObject* WorldContext, ETallowPickupSpotItem Kind, FName OnFloor, const TArray<FName>& ExcludeRooms,
		const TArray<ATallowPickupSpot*>& Taken, int32 Seed, bool bMustReach, FVector ReachFrom);

	/** How each item lies on a flat top before it is turned: the crowbar on its side with the claw flat,
	 * the fuse on its side, the matchbox flat. */
	static FRotator RestingRotation(ETallowPickupSpotItem Kind);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Spot")
	TObjectPtr<USceneComponent> Root;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category = "Spot")
	TObjectPtr<UStaticMeshComponent> Preview;
#endif
};
