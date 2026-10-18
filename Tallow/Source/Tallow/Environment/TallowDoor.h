#pragma once

#include "CoreMinimal.h"
#include "Components/TimelineComponent.h"
#include "GameFramework/Actor.h"
#include "Interactable/IInteractable.h"
#include "TallowDoor.generated.h"

class ATallowPlank;
class ATallowCharacter;
class UCurveFloat;

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
	
	/** Called during timeline update */
	UFUNCTION()
	void UpdateDoorRotation(float OutputValue);
	
	/** Called on interaction */
	void ToggleDoor(ATallowCharacter* Player);
	
	void ResetDoor();
	
	/** Overlap function */
	UFUNCTION()
	void OnAIProximityBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult );
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Tunables")
	float DoorSlamLoudness = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Tunables")
	float DoorOpenLoudness = 0.5f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Tunables")
	float DoorCloseLoudness = 0.5f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Tunables")
	float OpenAngle = 90.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Tunables")
	float DoorSlamPlayRate = 3.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Tunables")
	float DoorOpenPlayRate = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Tunables")
	float DoorClosePlayRate = 1.2f;
	
	FOnTimelineFloat TimelineProgress;
	
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
	
	TArray<TObjectPtr<ATallowPlank>> InitialPlanks;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	TObjectPtr<class UBoxComponent> AIProximityBox;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Timeline")
	TObjectPtr<UTimelineComponent> DoorTimeline;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Door|Timeline")
	TObjectPtr<UCurveFloat> DoorTimelineCurve;
	
	bool bIsOpen = false ;
	
	bool IsBarricaded() const { return BarricadePlanks.Num() > 0; }

};

