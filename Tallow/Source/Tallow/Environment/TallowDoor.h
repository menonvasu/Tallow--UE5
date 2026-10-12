#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable/IInteractable.h"
#include "TallowDoor.generated.h"

class UTimelineComponent;
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
	void ToggleDoor();
	
	void ResetDoor();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Tunables")
	float DoorSlamLoudness = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Tunables")
	float DoorOpenLoudness = 0.5f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Tunables")
	float DoorCloseLoudness = 0.5f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door|Tunables")
	float OpenAngle = 90.0f;
	
protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	TObjectPtr<UStaticMeshComponent> DoorFrameMesh;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	TObjectPtr<USceneComponent> HingePivot;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	UPROPERTY(EditInstanceOnly, Category = "Door|Barricade")
	TArray<TObjectPtr<class ATallowPlank>> BarricadePlanks;
	
	TArray<TObjectPtr<class ATallowPlank>> InitialPlanks;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Door|Timeline")
	TObjectPtr<UTimelineComponent> DoorTimeline;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Door|Timeline")
	TObjectPtr<UCurveFloat> DoorTimelineCurve;
	
	bool bIsOpen = false ;
	
	bool IsBarricaded() const { return BarricadePlanks.Num() > 0; }

};

