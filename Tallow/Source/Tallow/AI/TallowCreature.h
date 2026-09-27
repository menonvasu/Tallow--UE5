#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TallowCreature.generated.h"

class ATallowPatrolPoint;
class USphereComponent;
class UTallowCreatureAnimationComponent;
class UTallowSurfaceTraversalComponent;

/** The creature's two forms. Phase two is the spider form (#25) */
UENUM(BlueprintType)
enum class ETallowCreaturePhase : uint8
{
	PhaseOne	UMETA(DisplayName = "Phase One"),
	PhaseTwo	UMETA(DisplayName = "Phase Two")
};

UENUM(BlueprintType)
enum class ETallowCreatureState : uint8
{
	Patrol,
	Investigating,
	SuspiciousIdle,
	SearchingCorner,
	Chasing,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCreatureStateChanged, ETallowCreatureState, NewState);

UCLASS()
class TALLOW_API ATallowCreature : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this AI's properties
	ATallowCreature();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Keeper")
	TObjectPtr<UTallowCreatureAnimationComponent> KeeperAnimation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Keeper")
	TObjectPtr<UTallowSurfaceTraversalComponent> SurfaceTraversal;

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

	/** Switch form. Phase two can follow the player through child gaps (#23) */
	UFUNCTION(BlueprintCallable, Category = "AI|Phase")
	void SetPhase(ETallowCreaturePhase NewPhase);

	/** To set animation on state change*/
	UFUNCTION(BlueprintCallable, Category = "AI")
	void SetCreatureVisualState(ETallowCreatureState NewVisualState);

	/** Overriden to sweep around on inspecting points */
	virtual void GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const override;

	/** Get current form */
	UFUNCTION(BlueprintPure, Category = "AI|Phase")
	ETallowCreaturePhase GetPhase() const { return Phase; }

	UPROPERTY(BlueprintAssignable, Category = "AI|Events")
	FOnCreatureStateChanged OnCreatureStateChanged;

	UPROPERTY(VisibleAnywhere, Category = "AI|State")
	ETallowCreatureState CurrentState = ETallowCreatureState::Patrol;

protected:

	/** Current form. Set per instance to start a creature in phase two */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "AI|Phase")
	ETallowCreaturePhase Phase = ETallowCreaturePhase::PhaseOne;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FTransform InitialSpawnTransform;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<USphereComponent> GrabSphere;
};
