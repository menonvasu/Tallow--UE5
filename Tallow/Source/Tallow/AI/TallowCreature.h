#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TallowCreature.generated.h"

class ATallowPatrolPoint;
class USphereComponent;

UCLASS()
class TALLOW_API ATallowCreature : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this AI's properties
	ATallowCreature();

	/** Pawn BeginPlay implementation */
	virtual void BeginPlay() override;

	/** Reset to spawn */
	void ResetToSpawn();

	/** Overlap */
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
		
	/** Set starting patrol point */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "AI|Patrol")
	TObjectPtr<ATallowPatrolPoint> StartingPatrolPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FTransform InitialSpawnTransform;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<USphereComponent> GrabSphere;
};
