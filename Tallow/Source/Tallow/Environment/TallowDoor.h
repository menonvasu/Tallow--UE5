#pragma once

#include "CoreMinimal.h"
#include "Chapter/TallowChapterTypes.h"
#include "Components/TimelineComponent.h"
#include "GameFramework/Actor.h"
#include "Interactable/IInteractable.h"
#include "TallowDoor.generated.h"

class ATallowPlank;
class ATallowCharacter;
class UCurveFloat;
class UTallowDoorNavModifier;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDoorUnlocked);

UCLASS()
class TALLOW_API ATallowDoor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ATallowDoor();
	
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	
	virtual FText GetInteractionPromptText_Implementation(AActor* Interactor) const override;
	
	/** Remove plank from array */
	void NotifyPlankRemoved(ATallowPlank* Plank);
	
	virtual void OnInteract_Implementation(AActor* Interactor) override;
	
	virtual void OnInteractComplete_Implementation(AActor* Interactor) override;
	
	/** Called during timeline update */
	UFUNCTION()
	void UpdateDoorRotation(float OutputValue);
	
	/** Called on interaction */
	void ToggleDoor(ATallowCharacter* Player);
	
	void ResetDoor();

	/** Runtime lock changes update route planning with the physical leaf. */
	UFUNCTION(BlueprintCallable, Category = "Door|Lock")
	void SetLocked(bool bLocked);
	void ApplyChapterState(ETallowDoorState State);
	/** A bolted door answers a try with a short rattle and one of Nell's notes. */
	void RattleBolted(ATallowCharacter* Player);
	bool bChapterSealed = false;
	double LastRattleTime = -100.0;
	bool IsOpenForChapter() const { return bIsOpen; }
	bool HasChapterBarricade() const { return IsBarricaded(); }
	bool HasAuthoredPlanks() const { return !InitialPlanks.IsEmpty(); }
	
	/** Overlap function */
	UFUNCTION()
	void OnAIProximityBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult );
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Tunables")
	float DoorSlamLoudness = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Tunables")
	float DoorOpenLoudness = 0.5f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Tunables")
	float DoorCloseLoudness = 0.5f;

	/** The rattle of a chapter-sealed (bolted) door: quiet, and not often. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Tunables")
	float BoltedRattleLoudness = 0.2f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Tunables")
	float OpenAngle = 90.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Tunables")
	float DoorSlamPlayRate = 3.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Tunables")
	float DoorOpenPlayRate = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Tunables")
	float DoorClosePlayRate = 1.2f;
	
	UPROPERTY(EditInstanceOnly, Category = "Door|Lock")
	bool bIsLocked = false;

	/** A bolt, not a keyhole, on the room's side: locked from outside, but from inside it just opens. Pick your way
	 *  into a room once and its other door opens from within, so the house's loops come back. */
	UPROPERTY(EditInstanceOnly, Category = "Door|Lock")
	bool bOpensFromInside = false;

	/** Which way is into the room, in the door's own space (its +X by default). */
	UPROPERTY(EditInstanceOnly, Category = "Door|Lock", meta = (EditCondition = "bOpensFromInside"))
	FVector InsideDirection = FVector::ForwardVector;

	/** Whether this actor stands on the room side of the door. */
	bool IsInside(const AActor* Actor) const;
	
	FOnTimelineFloat TimelineProgress;
	
	UPROPERTY(BlueprintAssignable, EditAnywhere, Category = "Door|Events")
	FOnDoorUnlocked OnDoorUnlocked;
	
protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	TObjectPtr<UStaticMeshComponent> DoorFrameMesh;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	TObjectPtr<USceneComponent> HingePivot;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	UPROPERTY(EditInstanceOnly, Category = "Door|Barricade")
	TArray<TObjectPtr<ATallowPlank>> BarricadePlanks;
	
	UPROPERTY(Transient)
	TArray<TObjectPtr<ATallowPlank>> InitialPlanks;
	
	bool bInitiallyLocked = false;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	TObjectPtr<class UBoxComponent> AIProximityBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Navigation")
	TObjectPtr<UTallowDoorNavModifier> NavigationBarrier;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Timeline")
	TObjectPtr<UTimelineComponent> DoorTimeline;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Door|Timeline")
	TObjectPtr<UCurveFloat> DoorTimelineCurve;
	
	bool bIsOpen = false ;
	
	bool IsBarricaded() const { return BarricadePlanks.Num() > 0; }
	void UpdateNavigationBarrier();
	/** The open leaf, once it rests, is navigation geometry; a shut or moving one is not. */
	void UpdateLeafNavigation(bool bAtRestOpen);

};

